#include "AI/AIStrategyEcon.hpp"
#include "AI/AIBrain.hpp"
#include "AI/AITacticalEngine.hpp"
#include "AI/AITurnContext.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Systems/BuildSystem.hpp"
#include "Systems/ProductionSystem.hpp"
#include "Config/GameConfig.hpp"
#include "Config/AIConfig.hpp"
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

std::vector<TurnCommand> AIStrategyEcon::decide(Board& board, Kingdom& self,
                                                  Kingdom& enemy, const GameConfig& config,
                                                  const AIConfig& aiConfig, const AIBrain& brain,
                                                  AITacticalEngine& engine, const AITurnContext& ctx,
                                                  bool hasMoved, bool hasBuilt, bool hasProduced) {
    std::vector<TurnCommand> commands;
    const auto& priorities = brain.getPriorities();
    AIPhase phase = brain.getPhase();

    // =========================================================================
    // 1. Resource gathering: use cached resource cells + cached moves
    // =========================================================================
    if (!hasMoved && priorities.economy >= 0.3f) {
        if (!ctx.freeResourceCells.empty()) {
            float bestScore = -std::numeric_limits<float>::max();
            Piece* bestPiece = nullptr;
            sf::Vector2i bestTarget = {0, 0};

            for (auto& piece : self.pieces) {
                // Allow king to gather resources when we have very few pieces
                // (not just EARLY_GAME — if king is the only mobile piece, use it)
                if (piece.type == PieceType::King) {
                    int nonKingPieces = 0;
                    for (const auto& p : self.pieces)
                        if (p.type != PieceType::King) ++nonKingPieces;
                    if (nonKingPieces >= 2 && phase != AIPhase::EARLY_GAME) continue;
                }
                const Cell& curCell = board.getCell(piece.position.x, piece.position.y);
                if (curCell.building &&
                    (curCell.building->type == BuildingType::Mine || curCell.building->type == BuildingType::Farm))
                    continue;

                // Use cached move list
                auto it = ctx.selfMoves.find(piece.id);
                if (it == ctx.selfMoves.end()) continue;

                for (const auto& move : it->second) {
                    // O(1) safety check via cached threat map
                    if (piece.type != PieceType::King && !engine.isMoveSafe(move, ctx))
                        continue;

                    for (const auto& res : ctx.freeResourceCells) {
                        float dx = static_cast<float>(move.x - res.x);
                        float dy = static_cast<float>(move.y - res.y);
                        float dist = std::sqrt(dx * dx + dy * dy);

                        float score = -dist * 10.0f;
                        score += (500.0f - AITacticalEngine::pieceValue(piece.type));
                        const Cell& resCell = board.getCell(res.x, res.y);
                        if (resCell.building && resCell.building->type == BuildingType::Mine)
                            score += 100.0f;

                        if (score > bestScore) {
                            bestScore = score;
                            bestPiece = &piece;
                            bestTarget = move;
                        }
                    }
                }
            }

            if (bestPiece) {
                TurnCommand cmd;
                cmd.type = TurnCommand::Move;
                cmd.pieceId = bestPiece->id;
                cmd.origin = bestPiece->position;
                cmd.destination = bestTarget;
                commands.push_back(cmd);
            }
        }
    }

    // =========================================================================
    // 2. Build barracks if we don't have one
    // =========================================================================
    if (!hasBuilt && priorities.building >= 0.3f) {
        bool hasBarracks = false;
        for (const auto& b : self.buildings) {
            if (b.type == BuildingType::Barracks && !b.isDestroyed()) {
                hasBarracks = true;
                break;
            }
        }

        if (!hasBarracks && self.gold >= config.getBarracksCost()) {
            Piece* king = self.getKing();
            if (king) {
                // Try adjacent cells, expanding outward
                for (int radius = 1; radius <= 4; ++radius) {
                    for (int dy = -radius; dy <= radius; ++dy) {
                        for (int dx = -radius; dx <= radius; ++dx) {
                            if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
                            sf::Vector2i origin = king->position + sf::Vector2i(dx, dy);
                            if (BuildSystem::canBuild(BuildingType::Barracks, origin, *king, board, self, config)) {
                                TurnCommand cmd;
                                cmd.type = TurnCommand::Build;
                                cmd.buildingType = BuildingType::Barracks;
                                cmd.buildOrigin = origin;
                                commands.push_back(cmd);
                                goto barracks_done;
                            }
                        }
                    }
                }
                barracks_done:;
            }
        }
    }

    // =========================================================================
    // 3. Production: phase-aware army composition
    // =========================================================================
    if (!hasProduced && priorities.production >= 0.3f) {
        // Gold management: reserve some gold unless critical
        int goldReserve = 0;
        if (phase == AIPhase::MID_GAME || phase == AIPhase::BUILD_UP) goldReserve = 30;
        if (phase == AIPhase::EARLY_GAME) goldReserve = 0; // Spend everything early

        for (auto& b : self.buildings) {
            std::cerr << "    [Econ] Checking building id=" << b.id << " type=" << static_cast<int>(b.type)
                      << " destroyed=" << b.isDestroyed() << " producing=" << b.isProducing << std::endl;
            if (b.type != BuildingType::Barracks || b.isDestroyed() || b.isProducing) continue;

            // Count current army composition
            int pawns = 0, knights = 0, bishops = 0, rooks = 0, queens = 0;
            for (const auto& p : self.pieces) {
                switch (p.type) {
                    case PieceType::Pawn:   ++pawns;   break;
                    case PieceType::Knight: ++knights;  break;
                    case PieceType::Bishop: ++bishops;  break;
                    case PieceType::Rook:   ++rooks;    break;
                    case PieceType::Queen:  ++queens;   break;
                    default: break;
                }
            }

            // Phase-based production targets
            PieceType toProduce = PieceType::Pawn;

            if (phase == AIPhase::EARLY_GAME) {
                // Rush: Pawns first for economy, then a knight
                if (pawns < 2) toProduce = PieceType::Pawn;
                else if (knights < 1) toProduce = PieceType::Knight;
                else toProduce = PieceType::Pawn;
            } else if (phase == AIPhase::BUILD_UP) {
                // Build balanced army
                if (knights < 1) toProduce = PieceType::Knight;
                else if (bishops < 1) toProduce = PieceType::Bishop;
                else if (knights < 2) toProduce = PieceType::Knight;
                else if (rooks < 1 && self.gold >= config.getRecruitCost(PieceType::Rook) + goldReserve)
                    toProduce = PieceType::Rook;
                else toProduce = PieceType::Pawn;
            } else if (phase == AIPhase::MID_GAME || phase == AIPhase::AGGRESSION) {
                // Focus on strong pieces
                if (rooks < 1 && self.gold >= config.getRecruitCost(PieceType::Rook) + goldReserve)
                    toProduce = PieceType::Rook;
                else if (knights < 2) toProduce = PieceType::Knight;
                else if (bishops < 2) toProduce = PieceType::Bishop;
                else if (rooks < 2 && self.gold >= config.getRecruitCost(PieceType::Rook) + goldReserve)
                    toProduce = PieceType::Rook;
                else toProduce = PieceType::Knight;
            } else if (phase == AIPhase::ENDGAME) {
                // Need mating material
                if (!brain.hasSufficientMatingMaterial(self)) {
                    if (rooks < 1 && self.gold >= config.getRecruitCost(PieceType::Rook))
                        toProduce = PieceType::Rook;
                    else if (knights < 1) toProduce = PieceType::Knight;
                    else if (bishops < 1) toProduce = PieceType::Bishop;
                    else toProduce = PieceType::Knight;
                } else {
                    // Already have mating material, boost army
                    toProduce = PieceType::Knight;
                }
            } else {
                // CRISIS: produce whatever is cheapest and fastest
                toProduce = PieceType::Pawn;
            }

            // Check gold after reserve
            if (self.gold >= config.getRecruitCost(toProduce) + goldReserve) {
                if (ProductionSystem::canStartProduction(b, toProduce, self, config)) {
                    TurnCommand cmd;
                    cmd.type = TurnCommand::Produce;
                    cmd.barracksId = b.id;
                    cmd.produceType = toProduce;
                    commands.push_back(cmd);
                    break;
                }
            }
            // Fallback: try a cheaper unit if we can't afford the preferred one
            if (toProduce != PieceType::Pawn && self.gold >= config.getRecruitCost(PieceType::Pawn)) {
                if (ProductionSystem::canStartProduction(b, PieceType::Pawn, self, config)) {
                    TurnCommand cmd;
                    cmd.type = TurnCommand::Produce;
                    cmd.barracksId = b.id;
                    cmd.produceType = PieceType::Pawn;
                    commands.push_back(cmd);
                    break;
                }
            }
        }
    }

    return commands;
}
