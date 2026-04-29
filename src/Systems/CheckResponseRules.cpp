#include "Systems/CheckResponseRules.hpp"

#include <algorithm>

#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "Projection/ForwardModel.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/MarriageSystem.hpp"
#include "Systems/PendingTurnProjection.hpp"
#include "Systems/CheckSystem.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/TurnPointRules.hpp"
#include "Units/MovementRules.hpp"
#include "Units/Piece.hpp"

namespace {

bool hasQueuedMoveCommand(const std::vector<TurnCommand>& pendingCommands) {
    return std::any_of(pendingCommands.begin(), pendingCommands.end(),
        [](const TurnCommand& command) { return command.type == TurnCommand::Move; });
}

bool isPseudoLegalMove(const Piece& piece,
                       sf::Vector2i destination,
                       const Board& board,
                       const GameConfig& config) {
    if (!board.isInBounds(destination.x, destination.y)) {
        return false;
    }

    const Cell& destinationCell = board.getCell(destination.x, destination.y);
    if (destinationCell.piece
        && destinationCell.piece->kingdom != piece.kingdom
        && destinationCell.piece->type == PieceType::King) {
        return false;
    }

    const std::vector<sf::Vector2i> pseudoLegalMoves = MovementRules::getValidMoves(piece, board, config);
    return std::find(pseudoLegalMoves.begin(), pseudoLegalMoves.end(), destination) != pseudoLegalMoves.end();
}

void restorePendingMoveOrigins(Kingdom& activeKingdom,
                               const std::vector<TurnCommand>& pendingCommands) {
    for (const TurnCommand& command : pendingCommands) {
        if (command.type != TurnCommand::Move) {
            continue;
        }

        if (Piece* piece = activeKingdom.getPieceById(command.pieceId)) {
            piece->position = command.origin;
        }
    }
}

bool hasAnySnapshotLegalResponse(const GameSnapshot& snapshot,
                                 KingdomId activeKingdom,
                                 int globalMaxRange) {
    const SnapKingdom& kingdom = snapshot.kingdom(activeKingdom);
    for (const SnapPiece& piece : kingdom.pieces) {
        if (!ForwardModel::getLegalMoves(snapshot, piece, globalMaxRange).empty()) {
            return true;
        }
    }

    return false;
}

GameSnapshot simulateEndOfTurn(const GameSnapshot& snapshot,
                               KingdomId activeKingdom,
                               const GameConfig& config) {
    GameSnapshot simulatedSnapshot = snapshot.clone();
    ForwardModel::advanceTurn(simulatedSnapshot, activeKingdom, config);
    return simulatedSnapshot;
}

PendingTurnProjectionResult projectSingleCheckResponseMove(const GameSnapshot& currentSnapshot,
                                                           KingdomId activeKingdom,
                                                           const TurnCommand& command,
                                                           const GameConfig& config) {
    PendingTurnProjectionResult result;
    result.snapshot = currentSnapshot.clone();

    if (command.type != TurnCommand::Move) {
        result.valid = false;
        result.errorMessage = "Check: queue a move that gets the king out of check before any other action.";
        return result;
    }

    SnapPiece* piece = result.snapshot.kingdom(activeKingdom).getPieceById(command.pieceId);
    if (!piece) {
        result.valid = false;
        result.errorMessage = "The queued response move references a piece that no longer exists.";
        return result;
    }

    if (piece->position != command.origin) {
        result.valid = false;
        result.errorMessage = "The queued response move no longer matches the piece position.";
        return result;
    }

    const std::vector<sf::Vector2i> pseudoLegalMoves = ForwardModel::getPseudoLegalMoves(
        result.snapshot,
        *piece,
        config.getGlobalMaxRange());
    if (std::find(pseudoLegalMoves.begin(), pseudoLegalMoves.end(), command.destination)
        == pseudoLegalMoves.end()) {
        result.valid = false;
        result.errorMessage = "The queued response move is not geometrically legal.";
        return result;
    }

    PendingTurnProjection::initializeBudgets(result.snapshot, activeKingdom, config);
    SnapTurnBudget& budget = result.snapshot.turnBudget(activeKingdom);
    budget.movementPointsRemaining = std::max(
        budget.movementPointsRemaining,
        TurnPointRules::movementCost(piece->type, config));

    if (!ForwardModel::applyMove(result.snapshot,
                                 command.pieceId,
                                 command.destination,
                                 activeKingdom,
                                 config)) {
        result.valid = false;
        result.errorMessage = "The queued response move could not be applied.";
        return result;
    }

    return result;
}

} // namespace

bool CheckResponseRules::isActiveKingInCheck(const TurnValidationContext& context,
                                             const std::vector<TurnCommand>& pendingCommands) {
    const PendingTurnProjectionResult projection = PendingTurnProjection::project(
        context,
        pendingCommands);
    if (!projection.valid) {
        return true;
    }

    return ForwardModel::isInCheck(
        projection.snapshot, context.activeKingdom.id, context.config.getGlobalMaxRange());
}

bool CheckResponseRules::isActiveKingInCheck(const Kingdom& activeKingdom,
                                             const Kingdom& enemyKingdom,
                                             const Board& board,
                                             const std::vector<Building>& publicBuildings,
                                             int turnNumber,
                                             const std::vector<TurnCommand>& pendingCommands,
                                             const GameConfig& config) {
    return isActiveKingInCheck(
        TurnValidationContext{board, activeKingdom, enemyKingdom, publicBuildings, turnNumber, config},
        pendingCommands);
}

std::vector<sf::Vector2i> CheckResponseRules::filterLegalMovesForPiece(Piece& piece,
                                                                       Board& board,
                                                                       const GameConfig& config) {
    std::vector<sf::Vector2i> legalMoves;
    const std::vector<sf::Vector2i> pseudoLegalMoves = MovementRules::getValidMoves(piece, board, config);
    for (const sf::Vector2i& destination : pseudoLegalMoves) {
        if (!isPseudoLegalMove(piece, destination, board, config)) {
            continue;
        }

        if (moveKeepsKingSafe(piece, piece.position, destination, board, config)) {
            legalMoves.push_back(destination);
        }
    }

    return legalMoves;
}

bool CheckResponseRules::moveKeepsKingSafe(Piece& piece,
                                           sf::Vector2i origin,
                                           sf::Vector2i destination,
                                           Board& board,
                                           const GameConfig& config) {
    if (!board.isInBounds(origin.x, origin.y) || !board.isInBounds(destination.x, destination.y)) {
        return false;
    }

    Cell& originCell = board.getCell(origin.x, origin.y);
    Cell& destinationCell = board.getCell(destination.x, destination.y);
    if (originCell.piece != &piece) {
        return false;
    }

    Piece* capturedPiece = destinationCell.piece;
    const sf::Vector2i previousPosition = piece.position;

    originCell.piece = nullptr;
    destinationCell.piece = &piece;
    piece.position = destination;

    const bool stillInCheck = CheckSystem::isInCheck(piece.kingdom, board, config);

    piece.position = previousPosition;
    destinationCell.piece = capturedPiece;
    originCell.piece = &piece;

    return !stillInCheck;
}

bool CheckResponseRules::hasAnyLegalResponse(Kingdom& kingdom,
                                             Board& board,
                                             const GameConfig& config) {
    for (Piece& piece : kingdom.pieces) {
        if (!filterLegalMovesForPiece(piece, board, config).empty()) {
            return true;
        }
    }

    return false;
}

CheckTurnValidation CheckResponseRules::validatePendingTurn(const TurnValidationContext& context,
                                                            const std::vector<TurnCommand>& pendingCommands,
                                                            CheckEscapeSolver* sharedEscapeSolver) {
    (void) sharedEscapeSolver;

    CheckTurnValidation validation;
    Kingdom restoredActiveKingdom = context.activeKingdom;
    Kingdom restoredEnemyKingdom = context.enemyKingdom;
    restorePendingMoveOrigins(restoredActiveKingdom, pendingCommands);

    const GameSnapshot currentSnapshot = ForwardModel::createSnapshot(
        context.board,
        restoredActiveKingdom,
        restoredEnemyKingdom,
        context.publicBuildings,
        context.turnNumber,
        context.worldSeed,
        context.xpSystemState);

    validation.activeKingInCheck = ForwardModel::isInCheck(
        currentSnapshot, context.activeKingdom.id, context.config.getGlobalMaxRange());
    validation.projectedKingInCheck = validation.activeKingInCheck;
    validation.hasAnyLegalResponse = hasAnySnapshotLegalResponse(
        currentSnapshot, context.activeKingdom.id, context.config.getGlobalMaxRange());
    validation.requiresSingleResponseMove = validation.activeKingInCheck && validation.hasAnyLegalResponse;
    validation.hasQueuedMove = hasQueuedMoveCommand(pendingCommands);

    if (validation.activeKingInCheck && !validation.hasAnyLegalResponse) {
        validation.valid = false;
        validation.errorMessage = "Checkmate: the active kingdom has no legal response move.";
        return validation;
    }

    if (validation.activeKingInCheck) {
        if (pendingCommands.empty()) {
            validation.valid = false;
            validation.errorMessage = "Check: queue a move that gets the king out of check before any other action.";
            return validation;
        }
    }

    bool projectedKingInCheck = validation.activeKingInCheck;
    std::vector<TurnCommand> prefixCommands;
    prefixCommands.reserve(pendingCommands.size());
    PendingTurnProjectionResult finalProjection;
    const GameSnapshot* finalSnapshot = &currentSnapshot;
    if (!pendingCommands.empty()) {
        for (const TurnCommand& command : pendingCommands) {
            if (projectedKingInCheck && command.type != TurnCommand::Move) {
                validation.valid = false;
                validation.errorMessage = "Check: queue a move that gets the king out of check before any non-move action.";
                return validation;
            }

            if (projectedKingInCheck && command.type == TurnCommand::Move) {
                const PendingTurnProjectionResult responseProjection = projectSingleCheckResponseMove(
                    *finalSnapshot,
                    context.activeKingdom.id,
                    command,
                    context.config);
                if (!responseProjection.valid) {
                    validation.valid = false;
                    validation.errorMessage = responseProjection.errorMessage;
                    return validation;
                }

                projectedKingInCheck = ForwardModel::isInCheck(
                    responseProjection.snapshot,
                    context.activeKingdom.id,
                    context.config.getGlobalMaxRange());
                if (projectedKingInCheck) {
                    validation.valid = false;
                    validation.errorMessage = "The selected move does not get the king out of check.";
                    return validation;
                }

                prefixCommands.push_back(command);
                finalProjection.snapshot = std::move(responseProjection.snapshot);
                finalProjection.valid = true;
                finalSnapshot = &finalProjection.snapshot;
                continue;
            }

            prefixCommands.push_back(command);
            finalProjection = PendingTurnProjection::project(
                context,
                prefixCommands);
            if (!finalProjection.valid) {
                validation.valid = false;
                validation.errorMessage = finalProjection.errorMessage;
                return validation;
            }

            projectedKingInCheck = ForwardModel::isInCheck(
                finalProjection.snapshot, context.activeKingdom.id, context.config.getGlobalMaxRange());
        }

        finalSnapshot = &finalProjection.snapshot;
    }

    validation.projectedKingInCheck = projectedKingInCheck;

    if (projectedKingInCheck) {
        validation.valid = false;
        validation.errorMessage = validation.activeKingInCheck
            ? "The queued turn still leaves the king in check."
            : "The queued turn would leave the king in check.";
        return validation;
    }

    const GameSnapshot endOfTurnSnapshot = simulateEndOfTurn(
        *finalSnapshot,
        context.activeKingdom.id,
        context.config);
    validation.projectedEndingGold = endOfTurnSnapshot.kingdom(context.activeKingdom.id).gold;
    validation.bankrupt = validation.projectedEndingGold < 0;
    if (validation.bankrupt) {
        validation.valid = false;
        validation.errorMessage = "Bankruptcy: the kingdom would end the turn at "
            + std::to_string(validation.projectedEndingGold) + " gold.";
        return validation;
    }

    return validation;
}

CheckTurnValidation CheckResponseRules::validatePendingTurn(const Kingdom& activeKingdom,
                                const Kingdom& enemyKingdom,
                                const Board& board,
                                const std::vector<Building>& publicBuildings,
                                int turnNumber,
                                const std::vector<TurnCommand>& pendingCommands,
                                const GameConfig& config,
                                CheckEscapeSolver* sharedEscapeSolver) {
    return validatePendingTurn(
        TurnValidationContext{board, activeKingdom, enemyKingdom, publicBuildings, turnNumber, config},
    pendingCommands,
    sharedEscapeSolver);
}