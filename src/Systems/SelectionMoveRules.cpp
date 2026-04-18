#include "Systems/SelectionMoveRules.hpp"

#include <algorithm>

#include "Projection/ForwardModel.hpp"
#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/PendingTurnProjection.hpp"
#include "Systems/TurnPointRules.hpp"
#include "Units/Piece.hpp"

namespace {

std::size_t findPendingMoveCommandIndex(const std::vector<TurnCommand>& pendingCommands,
                                        int pieceId) {
    for (std::size_t index = 0; index < pendingCommands.size(); ++index) {
        const TurnCommand& command = pendingCommands[index];
        if (command.type == TurnCommand::Move && command.pieceId == pieceId) {
            return index;
        }
    }

    return pendingCommands.size();
}

std::vector<TurnCommand> pendingCommandsWithoutPieceLiveStateChanges(
    const std::vector<TurnCommand>& pendingCommands,
    int pieceId) {
    std::vector<TurnCommand> filteredCommands;
    filteredCommands.reserve(pendingCommands.size());

    for (const TurnCommand& command : pendingCommands) {
        if (command.type == TurnCommand::Move && command.pieceId == pieceId) {
            continue;
        }

        if (command.type == TurnCommand::Upgrade && command.upgradePieceId == pieceId) {
            continue;
        }

        filteredCommands.push_back(command);
    }

    return filteredCommands;
}

std::vector<TurnCommand> pendingCommandsBeforeIndex(const std::vector<TurnCommand>& pendingCommands,
                                                    std::size_t endIndex) {
    if (endIndex > pendingCommands.size()) {
        endIndex = pendingCommands.size();
    }

    return std::vector<TurnCommand>(pendingCommands.begin(), pendingCommands.begin() + endIndex);
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

TurnValidationContext makeSelectionContext(const TurnValidationContext& context,
                                           const Kingdom& activeKingdom) {
    return TurnValidationContext{
        context.board,
        activeKingdom,
        context.enemyKingdom,
        context.publicBuildings,
        context.turnNumber,
        context.config,
        context.worldSeed,
        context.xpSystemState};
}

PendingTurnProjectionResult projectSelectionState(const TurnValidationContext& context,
                                                  const std::vector<TurnCommand>& pendingCommands,
                                                  int pieceId) {
    PendingTurnProjectionResult result;

    Kingdom restoredActiveKingdom = context.activeKingdom;
    restorePendingMoveOrigins(restoredActiveKingdom, pendingCommands);

    const std::size_t pendingMoveIndex = findPendingMoveCommandIndex(pendingCommands, pieceId);
    const std::vector<TurnCommand> commandsToApply = pendingMoveIndex < pendingCommands.size()
        ? pendingCommandsBeforeIndex(pendingCommands, pendingMoveIndex)
        : pendingCommands;

    const PendingTurnNormalizationResult normalization = PendingTurnProjection::normalize(
        makeSelectionContext(context, restoredActiveKingdom),
        commandsToApply,
        PendingTurnInvalidCommandPolicy::FailFast);
    result.snapshot = normalization.snapshot;
    result.valid = normalization.valid;
    result.errorMessage = normalization.errorMessage;
    return result;
}

bool isPendingMoveOriginSelectable(const TurnValidationContext& context,
                                   const std::vector<TurnCommand>& pendingCommands,
                                   int pieceId) {
    if (findPendingMoveCommandIndex(pendingCommands, pieceId) >= pendingCommands.size()) {
        return true;
    }

    Kingdom restoredActiveKingdom = context.activeKingdom;
    restorePendingMoveOrigins(restoredActiveKingdom, pendingCommands);

    const PendingTurnNormalizationResult normalization = PendingTurnProjection::normalize(
        makeSelectionContext(context, restoredActiveKingdom),
        pendingCommandsWithoutPieceLiveStateChanges(pendingCommands, pieceId),
        PendingTurnInvalidCommandPolicy::DropInvalidBuilds);
    return normalization.valid;
}

} // namespace

bool SelectionMoveOptions::contains(sf::Vector2i destination) const {
    return std::find(safeMoves.begin(), safeMoves.end(), destination) != safeMoves.end()
        || std::find(unsafeMoves.begin(), unsafeMoves.end(), destination) != unsafeMoves.end();
}

SelectionMoveOptions SelectionMoveRules::classifyPieceMoves(const TurnValidationContext& context,
                                                            const std::vector<TurnCommand>& pendingCommands,
                                                            int pieceId) {
    SelectionMoveOptions moveOptions;
    moveOptions.originSelectable = isPendingMoveOriginSelectable(context, pendingCommands, pieceId);

    const PendingTurnProjectionResult projection = projectSelectionState(
        context,
        pendingCommands,
        pieceId);
    if (!projection.valid) {
        return moveOptions;
    }

    const SnapPiece* projectedPiece = projection.snapshot.kingdom(context.activeKingdom.id).getPieceById(pieceId);
    if (!projectedPiece) {
        return moveOptions;
    }

    moveOptions.originUnsafe = ForwardModel::isInCheck(
        projection.snapshot,
        context.activeKingdom.id,
        context.config.getGlobalMaxRange());

    const SnapTurnBudget& budget = projection.snapshot.turnBudget(context.activeKingdom.id);
    if (budget.moveCountForPiece(pieceId) >= TurnPointRules::moveAllowance(projectedPiece->type, context.config)) {
        return moveOptions;
    }
    if (budget.movementPointsRemaining < TurnPointRules::movementCost(projectedPiece->type, context.config)) {
        return moveOptions;
    }

    moveOptions.safeMoves = ForwardModel::getLegalMoves(
        projection.snapshot,
        *projectedPiece,
        context.config.getGlobalMaxRange());

    const std::vector<sf::Vector2i> pseudoLegalMoves = ForwardModel::getPseudoLegalMoves(
        projection.snapshot,
        *projectedPiece,
        context.config.getGlobalMaxRange());
    for (const sf::Vector2i& destination : pseudoLegalMoves) {
        if (std::find(moveOptions.safeMoves.begin(), moveOptions.safeMoves.end(), destination)
            == moveOptions.safeMoves.end()) {
            moveOptions.unsafeMoves.push_back(destination);
        }
    }

    return moveOptions;
}

SelectionMoveOptions SelectionMoveRules::classifyPieceMoves(const Board& board,
                                                            const Kingdom& activeKingdom,
                                                            const Kingdom& enemyKingdom,
                                                            const std::vector<Building>& publicBuildings,
                                                            int turnNumber,
                                                            const std::vector<TurnCommand>& pendingCommands,
                                                            int pieceId,
                                                            const GameConfig& config) {
    return classifyPieceMoves(
        TurnValidationContext{board, activeKingdom, enemyKingdom, publicBuildings, turnNumber, config},
        pendingCommands,
        pieceId);
}