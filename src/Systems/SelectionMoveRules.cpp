#include "Systems/SelectionMoveRules.hpp"

#include <algorithm>

#include "AI/ForwardModel.hpp"
#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/PendingTurnProjection.hpp"
#include "Systems/TurnPointRules.hpp"
#include "Units/Piece.hpp"

namespace {

const TurnCommand* findPendingMoveCommand(const std::vector<TurnCommand>& pendingCommands,
                                          int pieceId) {
    for (const TurnCommand& command : pendingCommands) {
        if (command.type == TurnCommand::Move && command.pieceId == pieceId) {
            return &command;
        }
    }

    return nullptr;
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

PendingTurnProjectionResult projectSelectionState(const TurnValidationContext& context,
                                                  const std::vector<TurnCommand>& pendingCommands,
                                                  int pieceId) {
    PendingTurnProjectionResult result;

    Kingdom restoredActiveKingdom = context.activeKingdom;
    if (const TurnCommand* pendingMove = findPendingMoveCommand(pendingCommands, pieceId)) {
        if (Piece* restoredPiece = restoredActiveKingdom.getPieceById(pieceId)) {
            restoredPiece->position = pendingMove->origin;
        }
    }

    const PendingTurnNormalizationResult normalization = PendingTurnProjection::normalize(
        TurnValidationContext{
            context.board,
            restoredActiveKingdom,
            context.enemyKingdom,
            context.publicBuildings,
            context.turnNumber,
            context.config},
        pendingCommandsWithoutPieceLiveStateChanges(pendingCommands, pieceId),
        PendingTurnInvalidCommandPolicy::DropInvalidBuilds);
    result.snapshot = normalization.snapshot;
    result.valid = normalization.valid;
    result.errorMessage = normalization.errorMessage;
    return result;
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