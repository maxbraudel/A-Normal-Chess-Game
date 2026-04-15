#include "Systems/CheckResponseRules.hpp"

#include <algorithm>

#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "AI/ForwardModel.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/PendingTurnProjection.hpp"
#include "Systems/CheckSystem.hpp"
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

bool hasAnyPseudoLegalResponse(const GameSnapshot& snapshot,
                               KingdomId activeKingdom,
                               int globalMaxRange) {
    const SnapKingdom& kingdom = snapshot.kingdom(activeKingdom);
    for (const SnapPiece& piece : kingdom.pieces) {
        if (!ForwardModel::getPseudoLegalMoves(snapshot, piece, globalMaxRange).empty()) {
            return true;
        }
    }

    return false;
}

} // namespace

bool CheckResponseRules::isActiveKingInCheck(const Kingdom& activeKingdom,
                                             const Kingdom& enemyKingdom,
                                             const Board& board,
                                             const std::vector<Building>& publicBuildings,
                                             int turnNumber,
                                             const std::vector<TurnCommand>& pendingCommands,
                                             const GameConfig& config) {
    const PendingTurnProjectionResult projection = PendingTurnProjection::project(
        board, activeKingdom, enemyKingdom, publicBuildings,
        turnNumber, pendingCommands, config);
    if (!projection.valid) {
        return true;
    }

    return ForwardModel::isInCheck(
        projection.snapshot, activeKingdom.id, config.getGlobalMaxRange());
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

CheckTurnValidation CheckResponseRules::validatePendingTurn(const Kingdom& activeKingdom,
                                                            const Kingdom& enemyKingdom,
                                                            const Board& board,
                                                            const std::vector<Building>& publicBuildings,
                                                            int turnNumber,
                                                            const std::vector<TurnCommand>& pendingCommands,
                                                            const GameConfig& config) {
    CheckTurnValidation validation;

    validation.activeKingInCheck = CheckSystem::isInCheckFast(
        activeKingdom, enemyKingdom, board, config);
    Kingdom activeCopy = activeKingdom;
    Board boardCopy = board;
    validation.hasAnyLegalResponse = hasAnyLegalResponse(activeCopy, boardCopy, config);
    validation.hasQueuedMove = hasQueuedMoveCommand(pendingCommands);

    if (validation.activeKingInCheck && !validation.hasAnyLegalResponse) {
        validation.valid = false;
        validation.errorMessage = "Checkmate: the active kingdom has no legal response move.";
        return validation;
    }

    if (pendingCommands.empty()) {
        if (validation.activeKingInCheck) {
            validation.valid = false;
            validation.errorMessage = "A kingdom in check cannot pass its turn and must resolve the check before ending the turn.";
        }
        return validation;
    }

    bool projectedKingInCheck = validation.activeKingInCheck;
    std::vector<TurnCommand> prefixCommands;
    prefixCommands.reserve(pendingCommands.size());
    for (const TurnCommand& command : pendingCommands) {
        if (projectedKingInCheck && command.type != TurnCommand::Move) {
            validation.valid = false;
            validation.errorMessage = "Non-move actions stay locked until the queued move sequence has resolved the check.";
            return validation;
        }

        prefixCommands.push_back(command);
        const PendingTurnProjectionResult prefixProjection = PendingTurnProjection::project(
            board, activeKingdom, enemyKingdom, publicBuildings,
            turnNumber, prefixCommands, config);
        if (!prefixProjection.valid) {
            validation.valid = false;
            validation.errorMessage = prefixProjection.errorMessage;
            return validation;
        }

        projectedKingInCheck = ForwardModel::isInCheck(
            prefixProjection.snapshot, activeKingdom.id, config.getGlobalMaxRange());
    }

    if (projectedKingInCheck) {
        validation.valid = false;
        validation.errorMessage = validation.activeKingInCheck
            ? "The queued turn still leaves the king in check."
            : "The queued turn would leave the king in check.";
        return validation;
    }

    return validation;
}