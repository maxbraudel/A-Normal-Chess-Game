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

std::vector<TurnCommand> pendingCommandsWithoutPieceMove(const std::vector<TurnCommand>& pendingCommands,
                                                         int pieceId) {
    std::vector<TurnCommand> filteredCommands;
    filteredCommands.reserve(pendingCommands.size());

    for (const TurnCommand& command : pendingCommands) {
        if (command.type == TurnCommand::Move && command.pieceId == pieceId) {
            continue;
        }

        filteredCommands.push_back(command);
    }

    return filteredCommands;
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

PendingTurnProjectionResult projectSelectionState(const Board& board,
                                                  const Kingdom& activeKingdom,
                                                  const Kingdom& enemyKingdom,
                                                  const std::vector<Building>& publicBuildings,
                                                  int turnNumber,
                                                  const std::vector<TurnCommand>& pendingCommands,
                                                  int pieceId,
                                                  const GameConfig& config) {
    PendingTurnProjectionResult result;

    Kingdom restoredActiveKingdom = activeKingdom;
    if (const TurnCommand* pendingMove = findPendingMoveCommand(pendingCommands, pieceId)) {
        if (Piece* restoredPiece = restoredActiveKingdom.getPieceById(pieceId)) {
            restoredPiece->position = pendingMove->origin;
        }
    }

    const PendingTurnNormalizationResult normalization = PendingTurnProjection::normalize(
        board,
        restoredActiveKingdom,
        enemyKingdom,
        publicBuildings,
        turnNumber,
        pendingCommandsWithoutPieceLiveStateChanges(pendingCommands, pieceId),
        config,
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

SelectionMoveOptions SelectionMoveRules::classifyPieceMoves(const Board& board,
                                                            const Kingdom& activeKingdom,
                                                            const Kingdom& enemyKingdom,
                                                            const std::vector<Building>& publicBuildings,
                                                            int turnNumber,
                                                            const std::vector<TurnCommand>& pendingCommands,
                                                            int pieceId,
                                                            const GameConfig& config) {
    SelectionMoveOptions moveOptions;

    const PendingTurnProjectionResult projection = projectSelectionState(
        board,
        activeKingdom,
        enemyKingdom,
        publicBuildings,
        turnNumber,
        pendingCommands,
        pieceId,
        config);
    if (!projection.valid) {
        return moveOptions;
    }

    const SnapPiece* projectedPiece = projection.snapshot.kingdom(activeKingdom.id).getPieceById(pieceId);
    if (!projectedPiece) {
        return moveOptions;
    }

    moveOptions.originUnsafe = ForwardModel::isInCheck(
        projection.snapshot,
        activeKingdom.id,
        config.getGlobalMaxRange());

    const SnapTurnBudget& budget = projection.snapshot.turnBudget(activeKingdom.id);
    if (budget.moveCountForPiece(pieceId) >= TurnPointRules::moveAllowance(projectedPiece->type, config)) {
        return moveOptions;
    }
    if (budget.movementPointsRemaining < TurnPointRules::movementCost(projectedPiece->type, config)) {
        return moveOptions;
    }

    moveOptions.safeMoves = ForwardModel::getLegalMoves(
        projection.snapshot,
        *projectedPiece,
        config.getGlobalMaxRange());

    const std::vector<sf::Vector2i> pseudoLegalMoves = ForwardModel::getPseudoLegalMoves(
        projection.snapshot,
        *projectedPiece,
        config.getGlobalMaxRange());
    for (const sf::Vector2i& destination : pseudoLegalMoves) {
        if (std::find(moveOptions.safeMoves.begin(), moveOptions.safeMoves.end(), destination)
            == moveOptions.safeMoves.end()) {
            moveOptions.unsafeMoves.push_back(destination);
        }
    }

    return moveOptions;
}