#include "AI/AIStrategyEcon.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Units/MovementRules.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Systems/BuildSystem.hpp"
#include "Systems/ProductionSystem.hpp"
#include "Config/GameConfig.hpp"
#include "Config/AIConfig.hpp"
#include <cmath>
#include <limits>

std::vector<TurnCommand> AIStrategyEcon::decide(const Board& board, Kingdom& self,
                                                  const Kingdom& enemy, const GameConfig& config,
                                                  const AIConfig& aiConfig, bool hasMoved,
                                                  bool hasBuilt, bool hasProduced) {
    std::vector<TurnCommand> commands;

    // 1. Check if we have income — find pieces on mines/farms
    bool hasIncome = false;
    for (const auto& p : self.pieces) {
        const Cell& cell = board.getCell(p.position.x, p.position.y);
        if (cell.building && (cell.building->type == BuildingType::Mine || cell.building->type == BuildingType::Farm)) {
            hasIncome = true;
            break;
        }
    }

    // 2. If no income and not moved yet, move a piece toward nearest mine/farm
    if (!hasIncome && !hasMoved) {
        float bestDist = std::numeric_limits<float>::max();
        Piece* bestPiece = nullptr;
        sf::Vector2i bestTarget = {0, 0};

        // Find all mine/farm cells
        std::vector<sf::Vector2i> resourceCells;
        int diam = board.getDiameter();
        for (int y = 0; y < diam; ++y) {
            for (int x = 0; x < diam; ++x) {
                const Cell& cell = board.getCell(x, y);
                if (cell.isInCircle && cell.building && (cell.building->type == BuildingType::Mine || cell.building->type == BuildingType::Farm)) {
                    // Not occupied by enemy
                    if (!enemy.getPieceAt({x, y})) {
                        resourceCells.push_back({x, y});
                    }
                }
            }
        }

        for (auto& piece : self.pieces) {
            auto moves = MovementRules::getValidMoves(piece, board, config);
            for (const auto& move : moves) {
                for (const auto& res : resourceCells) {
                    float dx = static_cast<float>(move.x - res.x);
                    float dy = static_cast<float>(move.y - res.y);
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist < bestDist) {
                        bestDist = dist;
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

    // 3. If enough gold and no barracks, build one
    if (!hasBuilt) {
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
                // Try adjacent cells for barracks placement
                sf::Vector2i offsets[] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,-1},{1,-1},{-1,1},
                                          {2,0},{-2,0},{0,2},{0,-2}};
                for (auto& off : offsets) {
                    sf::Vector2i origin = king->position + off;
                    if (BuildSystem::canBuild(BuildingType::Barracks, origin, *king, board, self, config)) {
                        TurnCommand cmd;
                        cmd.type = TurnCommand::Build;
                        cmd.buildingType = BuildingType::Barracks;
                        cmd.buildOrigin = origin;
                        commands.push_back(cmd);
                        break;
                    }
                }
            }
        }
    }

    // 4. If barracks free and enough gold, start production
    if (!hasProduced) {
        for (auto& b : self.buildings) {
            if (b.type == BuildingType::Barracks && !b.isDestroyed() && !b.isProducing) {
                // Decide what to produce based on army composition
                PieceType toProduce = PieceType::Pawn;
                int pawnCount = 0, knightCount = 0, bishopCount = 0, rookCount = 0;
                for (const auto& p : self.pieces) {
                    switch (p.type) {
                        case PieceType::Pawn: pawnCount++; break;
                        case PieceType::Knight: knightCount++; break;
                        case PieceType::Bishop: bishopCount++; break;
                        case PieceType::Rook: rookCount++; break;
                        default: break;
                    }
                }
                if (knightCount < 2) toProduce = PieceType::Knight;
                else if (rookCount < 1) toProduce = PieceType::Rook;
                else if (bishopCount < 2) toProduce = PieceType::Bishop;
                else toProduce = PieceType::Pawn;

                if (ProductionSystem::canStartProduction(b, toProduce, self, config)) {
                    TurnCommand cmd;
                    cmd.type = TurnCommand::Produce;
                    cmd.barracksId = b.id;
                    cmd.produceType = toProduce;
                    commands.push_back(cmd);
                    break;
                }
            }
        }
    }

    return commands;
}
