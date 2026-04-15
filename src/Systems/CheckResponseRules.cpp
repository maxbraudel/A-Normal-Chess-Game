#include "Systems/CheckResponseRules.hpp"

#include <algorithm>

#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/CheckSystem.hpp"
#include "Units/MovementRules.hpp"
#include "Units/Piece.hpp"

namespace {

const TurnCommand* findPendingMoveCommand(const std::vector<TurnCommand>& pendingCommands) {
    for (const TurnCommand& command : pendingCommands) {
        if (command.type == TurnCommand::Move) {
            return &command;
        }
    }

    return nullptr;
}

bool containsIllegalNonMoveAction(const std::vector<TurnCommand>& pendingCommands) {
    return std::any_of(
        pendingCommands.begin(), pendingCommands.end(),
        [](const TurnCommand& command) { return command.type != TurnCommand::Move; });
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

class PendingMovePreviewGuard {
public:
    PendingMovePreviewGuard(Kingdom& kingdom, const std::vector<TurnCommand>& pendingCommands)
        : m_piece(nullptr), m_originalPosition(0, 0) {
        const TurnCommand* moveCommand = findPendingMoveCommand(pendingCommands);
        if (!moveCommand) {
            return;
        }

        m_piece = kingdom.getPieceById(moveCommand->pieceId);
        if (!m_piece) {
            return;
        }

        m_originalPosition = m_piece->position;
        m_piece->position = moveCommand->origin;
    }

    ~PendingMovePreviewGuard() {
        if (m_piece) {
            m_piece->position = m_originalPosition;
        }
    }

private:
    Piece* m_piece;
    sf::Vector2i m_originalPosition;
};

} // namespace

bool CheckResponseRules::isActiveKingInCheck(Kingdom& kingdom,
                                             Board& board,
                                             const std::vector<TurnCommand>& pendingCommands,
                                             const GameConfig& config) {
    PendingMovePreviewGuard previewGuard(kingdom, pendingCommands);
    return CheckSystem::isInCheck(kingdom.id, board, config);
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

CheckTurnValidation CheckResponseRules::validatePendingTurn(Kingdom& activeKingdom,
                                                            Board& board,
                                                            const std::vector<TurnCommand>& pendingCommands,
                                                            const GameConfig& config) {
    CheckTurnValidation validation;
    PendingMovePreviewGuard previewGuard(activeKingdom, pendingCommands);

    validation.activeKingInCheck = CheckSystem::isInCheck(activeKingdom.id, board, config);
    validation.hasAnyLegalResponse = hasAnyLegalResponse(activeKingdom, board, config);

    const TurnCommand* moveCommand = findPendingMoveCommand(pendingCommands);
    validation.hasQueuedMove = (moveCommand != nullptr);

    if (validation.activeKingInCheck && containsIllegalNonMoveAction(pendingCommands)) {
        validation.valid = false;
        validation.errorMessage = "A kingdom in check may only submit a move that resolves the check.";
        return validation;
    }

    if (validation.activeKingInCheck && !validation.hasAnyLegalResponse) {
        validation.valid = false;
        validation.errorMessage = "Checkmate: the active kingdom has no legal response move.";
        return validation;
    }

    if (!moveCommand) {
        if (validation.activeKingInCheck) {
            validation.valid = false;
            validation.errorMessage = "A kingdom in check cannot pass its turn and must resolve the check with a legal move.";
        }
        return validation;
    }

    Piece* movingPiece = activeKingdom.getPieceById(moveCommand->pieceId);
    if (!movingPiece) {
        validation.valid = false;
        validation.errorMessage = "The queued move references a piece that does not exist anymore.";
        return validation;
    }

    if (!isPseudoLegalMove(*movingPiece, moveCommand->destination, board, config)) {
        validation.valid = false;
        validation.errorMessage = "The queued move is not geometrically legal for the selected piece.";
        return validation;
    }

    if (!moveKeepsKingSafe(*movingPiece,
                           moveCommand->origin,
                           moveCommand->destination,
                           board,
                           config)) {
        validation.valid = false;
        validation.errorMessage = validation.activeKingInCheck
            ? "The queued move does not resolve the current check."
            : "The queued move would leave the king in check.";
        return validation;
    }

    return validation;
}