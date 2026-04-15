#include "Systems/PendingTurnProjection.hpp"

#include <algorithm>

#include "AI/ForwardModel.hpp"
#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/StructureIntegrityRules.hpp"
#include "Systems/TurnPointRules.hpp"

namespace {

SnapTurnBudget toSnapTurnBudget(const TurnPointBudget& budget) {
    SnapTurnBudget snapBudget;
    snapBudget.movementPointsMax = budget.movementPointsMax;
    snapBudget.movementPointsRemaining = budget.movementPointsRemaining;
    snapBudget.buildPointsMax = budget.buildPointsMax;
    snapBudget.buildPointsRemaining = budget.buildPointsRemaining;
    return snapBudget;
}

int getGoldBuildCost(BuildingType type, const GameConfig& config) {
    switch (type) {
        case BuildingType::Barracks:
            return config.getBarracksCost();
        case BuildingType::WoodWall:
            return config.getWoodWallCost();
        case BuildingType::StoneWall:
            return config.getStoneWallCost();
        case BuildingType::Arena:
            return config.getArenaCost();
        default:
            return 0;
    }
}

bool canUpgradeSnapshotPiece(const SnapPiece& piece,
                             PieceType target,
                             const GameConfig& config) {
    if (piece.type == PieceType::Pawn) {
        if (target == PieceType::Knight || target == PieceType::Bishop) {
            return piece.xp >= config.getXPThresholdPawnToKnightOrBishop();
        }
    }

    if (piece.type == PieceType::Knight || piece.type == PieceType::Bishop) {
        if (target == PieceType::Rook) {
            return piece.xp >= config.getXPThresholdToRook();
        }
    }

    return false;
}

void restorePendingMoveOrigins(Kingdom& activeKingdom,
                               const std::vector<TurnCommand>& commands) {
    for (const TurnCommand& command : commands) {
        if (command.type != TurnCommand::Move) {
            continue;
        }

        if (Piece* piece = activeKingdom.getPieceById(command.pieceId)) {
            piece->position = command.origin;
        }
    }
}

bool applyProjectedCommand(GameSnapshot& snapshot,
                           KingdomId activeKingdom,
                           const TurnCommand& command,
                           const GameConfig& config,
                           std::string* errorMessage) {
    switch (command.type) {
        case TurnCommand::Move: {
            const SnapPiece* piece = snapshot.kingdom(activeKingdom).getPieceById(command.pieceId);
            if (!piece) {
                if (errorMessage) {
                    *errorMessage = "The queued move references a piece that no longer exists.";
                }
                return false;
            }

            const auto pseudoLegalMoves = ForwardModel::getPseudoLegalMoves(
                snapshot, *piece, config.getGlobalMaxRange());
            if (std::find(pseudoLegalMoves.begin(), pseudoLegalMoves.end(), command.destination)
                == pseudoLegalMoves.end()) {
                if (errorMessage) {
                    *errorMessage = "The queued move is not geometrically legal in the projected turn state.";
                }
                return false;
            }

            if (!ForwardModel::applyMove(snapshot, command.pieceId, command.destination,
                                         activeKingdom, config)) {
                if (errorMessage) {
                    *errorMessage = "The queued move exceeds movement points or piece move allowance.";
                }
                return false;
            }

            return true;
        }

        case TurnCommand::Build: {
            const int sourceWidth = config.getBuildingWidth(command.buildingType);
            const int sourceHeight = config.getBuildingHeight(command.buildingType);
            const int buildGoldCost = getGoldBuildCost(command.buildingType, config);
            const int defaultCellHP = StructureIntegrityRules::defaultCellHP(command.buildingType, config);
            if (!ForwardModel::applyBuild(snapshot, activeKingdom, command.buildingType,
                                          command.buildOrigin, sourceWidth, sourceHeight,
                                          command.buildRotationQuarterTurns, buildGoldCost,
                                          defaultCellHP, config)) {
                if (errorMessage) {
                    *errorMessage = "The queued build is not legal in the projected turn state or exceeds available build points.";
                }
                return false;
            }

            return true;
        }

        case TurnCommand::Produce: {
            if (!ForwardModel::applyProduce(snapshot, command.barracksId, command.produceType,
                                            config.getRecruitCost(command.produceType),
                                            config.getProductionTurns(command.produceType),
                                            activeKingdom)) {
                if (errorMessage) {
                    *errorMessage = "The queued production is not legal in the projected turn state.";
                }
                return false;
            }

            return true;
        }

        case TurnCommand::Upgrade: {
            SnapKingdom& kingdom = snapshot.kingdom(activeKingdom);
            SnapPiece* piece = kingdom.getPieceById(command.upgradePieceId);
            if (!piece) {
                if (errorMessage) {
                    *errorMessage = "The queued upgrade references a piece that no longer exists.";
                }
                return false;
            }

            const int goldCost = config.getUpgradeCost(piece->type, command.upgradeTarget);
            if (goldCost <= 0 || kingdom.gold < goldCost
                || !canUpgradeSnapshotPiece(*piece, command.upgradeTarget, config)) {
                if (errorMessage) {
                    *errorMessage = "The queued upgrade is not legal in the projected turn state.";
                }
                return false;
            }

            kingdom.gold -= goldCost;
            piece->type = command.upgradeTarget;
            return true;
        }

        case TurnCommand::Marry:
            if (!ForwardModel::applyMarriage(snapshot, activeKingdom)) {
                if (errorMessage) {
                    *errorMessage = "The queued marriage is not legal in the projected turn state.";
                }
                return false;
            }

            return true;

        case TurnCommand::FormGroup:
        case TurnCommand::BreakGroup:
        default:
            return true;
    }
}

} // namespace

void PendingTurnProjection::initializeBudgets(GameSnapshot& snapshot,
                                              KingdomId activeKingdom,
                                              const GameConfig& config) {
    snapshot.turnBudget(activeKingdom) = toSnapTurnBudget(TurnPointRules::makeBudget(config));
    snapshot.turnBudget(opponent(activeKingdom)) = toSnapTurnBudget(TurnPointRules::makeBudget(config));
}

PendingTurnProjectionResult PendingTurnProjection::project(
    const Board& board,
    const Kingdom& activeKingdom,
    const Kingdom& enemyKingdom,
    const std::vector<Building>& publicBuildings,
    int turnNumber,
    const std::vector<TurnCommand>& commands,
    const GameConfig& config) {
    PendingTurnProjectionResult result;

    Kingdom restoredActiveKingdom = activeKingdom;
    Kingdom restoredEnemyKingdom = enemyKingdom;
    restorePendingMoveOrigins(restoredActiveKingdom, commands);

    result.snapshot = ForwardModel::createSnapshot(
        board, restoredActiveKingdom, restoredEnemyKingdom, publicBuildings, turnNumber);
    initializeBudgets(result.snapshot, activeKingdom.id, config);

    for (const TurnCommand& command : commands) {
        if (!applyProjectedCommand(result.snapshot, activeKingdom.id, command, config,
                                   &result.errorMessage)) {
            result.valid = false;
            return result;
        }
    }

    return result;
}

PendingTurnProjectionResult PendingTurnProjection::projectWithCandidate(
    const Board& board,
    const Kingdom& activeKingdom,
    const Kingdom& enemyKingdom,
    const std::vector<Building>& publicBuildings,
    int turnNumber,
    const std::vector<TurnCommand>& commands,
    const TurnCommand& candidate,
    const GameConfig& config) {
    std::vector<TurnCommand> projectedCommands = commands;
    projectedCommands.push_back(candidate);
    return project(board, activeKingdom, enemyKingdom, publicBuildings,
                   turnNumber, projectedCommands, config);
}

bool PendingTurnProjection::canAppendCommand(
    const Board& board,
    const Kingdom& activeKingdom,
    const Kingdom& enemyKingdom,
    const std::vector<Building>& publicBuildings,
    int turnNumber,
    const std::vector<TurnCommand>& commands,
    const TurnCommand& candidate,
    const GameConfig& config,
    std::string* errorMessage) {
    const PendingTurnProjectionResult projection = projectWithCandidate(
        board, activeKingdom, enemyKingdom, publicBuildings,
        turnNumber, commands, candidate, config);
    if (!projection.valid && errorMessage) {
        *errorMessage = projection.errorMessage;
    }

    return projection.valid;
}

std::vector<sf::Vector2i> PendingTurnProjection::projectedPseudoLegalMovesForPiece(
    const Board& board,
    const Kingdom& activeKingdom,
    const Kingdom& enemyKingdom,
    const std::vector<Building>& publicBuildings,
    int turnNumber,
    const std::vector<TurnCommand>& commands,
    int pieceId,
    const GameConfig& config) {
    const PendingTurnProjectionResult projection = project(
        board, activeKingdom, enemyKingdom, publicBuildings, turnNumber, commands, config);
    if (!projection.valid) {
        return {};
    }

    const SnapPiece* piece = projection.snapshot.kingdom(activeKingdom.id).getPieceById(pieceId);
    if (!piece) {
        return {};
    }

    const SnapTurnBudget& budget = projection.snapshot.turnBudget(activeKingdom.id);
    if (budget.moveCountForPiece(pieceId) >= TurnPointRules::moveAllowance(piece->type, config)) {
        return {};
    }
    if (budget.movementPointsRemaining < TurnPointRules::movementCost(piece->type, config)) {
        return {};
    }

    return ForwardModel::getPseudoLegalMoves(
        projection.snapshot, *piece, config.getGlobalMaxRange());
}