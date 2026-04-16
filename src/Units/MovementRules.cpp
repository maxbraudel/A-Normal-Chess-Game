#include "Units/MovementRules.hpp"
#include "Units/Piece.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"

namespace {

bool isBlockingWallCell(const Cell& cell, int worldX, int worldY) {
    if (!cell.building) {
        return false;
    }

    const int localX = worldX - cell.building->origin.x;
    const int localY = worldY - cell.building->origin.y;
    return cell.building->isWall()
        && !cell.building->isCellDestroyed(localX, localY);
}

bool isEnemyCapturableBuildingCell(const Cell& cell, KingdomId mover) {
    return cell.building && !cell.building->isNeutral && cell.building->owner != mover;
}

bool isPawnBoardDestinationTraversable(const Board& board, int x, int y) {
    if (!board.isInBounds(x, y)) {
        return false;
    }

    const Cell& cell = board.getCell(x, y);
    return cell.isInCircle && cell.type != CellType::Water;
}

}

std::vector<sf::Vector2i> MovementRules::getValidMoves(
    const Piece& piece, const Board& board, const GameConfig& config) {
    switch (piece.type) {
        case PieceType::Pawn:   return getPawnMoves(piece, board);
        case PieceType::Knight: return getKnightMoves(piece, board, config);
        case PieceType::Bishop: return getBishopMoves(piece, board, config);
        case PieceType::Rook:   return getRookMoves(piece, board, config);
        case PieceType::Queen:  return getQueenMoves(piece, board, config);
        case PieceType::King:   return getKingMoves(piece, board);
    }
    return {};
}

std::vector<sf::Vector2i> MovementRules::getThreatenedSquares(
    const Piece& piece, const Board& board, const GameConfig& config) {
    if (piece.type == PieceType::Pawn) {
        return getPawnThreatenedSquares(piece, board);
    }

    return getValidMoves(piece, board, config);
}

std::vector<sf::Vector2i> MovementRules::getPawnMoves(const Piece& piece, const Board& board) {
    std::vector<sf::Vector2i> moves;
    static const int orthogonalDirs[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    static const int diagonalDirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    for (const auto& direction : orthogonalDirs) {
        const int nx = piece.position.x + direction[0];
        const int ny = piece.position.y + direction[1];
        if (!isPawnBoardDestinationTraversable(board, nx, ny)) {
            continue;
        }

        const Cell& cell = board.getCell(nx, ny);
        if (cell.piece) {
            continue;
        }
        if (isEnemyCapturableBuildingCell(cell, piece.kingdom)) {
            continue;
        }
        if (isBlockingWallCell(cell, nx, ny)) {
            continue;
        }
        moves.push_back({nx, ny});
    }

    for (const auto& direction : diagonalDirs) {
        const int nx = piece.position.x + direction[0];
        const int ny = piece.position.y + direction[1];
        if (!isPawnBoardDestinationTraversable(board, nx, ny)) {
            continue;
        }

        const Cell& cell = board.getCell(nx, ny);
        if (cell.piece && cell.piece->kingdom != piece.kingdom) {
            moves.push_back({nx, ny});
            continue;
        }
        if (isEnemyCapturableBuildingCell(cell, piece.kingdom)) {
            moves.push_back({nx, ny});
        }
    }

    return moves;
}

std::vector<sf::Vector2i> MovementRules::getPawnThreatenedSquares(const Piece& piece, const Board& board) {
    std::vector<sf::Vector2i> moves;
    static const int diagonalDirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    for (const auto& direction : diagonalDirs) {
        const int nx = piece.position.x + direction[0];
        const int ny = piece.position.y + direction[1];
        if (!isPawnBoardDestinationTraversable(board, nx, ny)) {
            continue;
        }

        moves.push_back({nx, ny});
    }

    return moves;
}

std::vector<sf::Vector2i> MovementRules::getKnightMoves(const Piece& piece, const Board& board, const GameConfig& config) {
    std::vector<sf::Vector2i> moves;
    const int dx[] = {1, 2, 2, 1, -1, -2, -2, -1};
    const int dy[] = {-2, -1, 1, 2, 2, 1, -1, -2};

    for (int d = 0; d < 8; ++d) {
        int nx = piece.position.x + dx[d];
        int ny = piece.position.y + dy[d];
        if (!board.isInBounds(nx, ny)) continue;
        const Cell& cell = board.getCell(nx, ny);
        if (!cell.isInCircle || cell.type == CellType::Water) continue;

        if (isBlockingWallCell(cell, nx, ny)) {
            if (cell.building->owner != piece.kingdom && !cell.building->isNeutral)
                moves.push_back({nx, ny});
            continue;
        }
        if (cell.piece && cell.piece->kingdom == piece.kingdom) continue;
        moves.push_back({nx, ny});
    }
    return moves;
}

std::vector<sf::Vector2i> MovementRules::getBishopMoves(const Piece& piece, const Board& board, const GameConfig& config) {
    std::vector<sf::Vector2i> moves;
    int maxRange = config.getGlobalMaxRange();
    const int dx[] = {1, 1, -1, -1};
    const int dy[] = {1, -1, 1, -1};
    for (int d = 0; d < 4; ++d) {
        auto traced = traceDirection(piece, board, dx[d], dy[d], maxRange);
        moves.insert(moves.end(), traced.begin(), traced.end());
    }
    return moves;
}

std::vector<sf::Vector2i> MovementRules::getRookMoves(const Piece& piece, const Board& board, const GameConfig& config) {
    std::vector<sf::Vector2i> moves;
    int maxRange = config.getGlobalMaxRange();
    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};
    for (int d = 0; d < 4; ++d) {
        auto traced = traceDirection(piece, board, dx[d], dy[d], maxRange);
        moves.insert(moves.end(), traced.begin(), traced.end());
    }
    return moves;
}

std::vector<sf::Vector2i> MovementRules::getQueenMoves(const Piece& piece, const Board& board, const GameConfig& config) {
    std::vector<sf::Vector2i> moves;
    int maxRange = config.getGlobalMaxRange();
    const int dx[] = {0, 0, 1, -1, 1, 1, -1, -1};
    const int dy[] = {1, -1, 0, 0, 1, -1, 1, -1};
    for (int d = 0; d < 8; ++d) {
        auto traced = traceDirection(piece, board, dx[d], dy[d], maxRange);
        moves.insert(moves.end(), traced.begin(), traced.end());
    }
    return moves;
}

std::vector<sf::Vector2i> MovementRules::getKingMoves(const Piece& piece, const Board& board) {
    std::vector<sf::Vector2i> moves;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = piece.position.x + dx;
            int ny = piece.position.y + dy;
            if (!board.isInBounds(nx, ny)) continue;
            const Cell& cell = board.getCell(nx, ny);
            if (!cell.isInCircle || cell.type == CellType::Water) continue;

            if (isBlockingWallCell(cell, nx, ny)) {
                if (cell.building->owner != piece.kingdom && !cell.building->isNeutral)
                    moves.push_back({nx, ny});
                continue;
            }
            if (cell.piece && cell.piece->kingdom == piece.kingdom) continue;
            moves.push_back({nx, ny});
        }
    }
    return moves;
}

std::vector<sf::Vector2i> MovementRules::traceDirection(
    const Piece& piece, const Board& board, int dx, int dy, int maxRange) {
    std::vector<sf::Vector2i> moves;
    for (int step = 1; step <= maxRange; ++step) {
        int nx = piece.position.x + dx * step;
        int ny = piece.position.y + dy * step;
        if (!board.isInBounds(nx, ny)) break;
        const Cell& cell = board.getCell(nx, ny);
        if (!cell.isInCircle || cell.type == CellType::Water) break;

        if (isBlockingWallCell(cell, nx, ny)) {
            if (cell.building->owner != piece.kingdom && !cell.building->isNeutral)
                moves.push_back({nx, ny});
            break;
        }
        if (cell.piece && cell.piece->kingdom == piece.kingdom) break;
        moves.push_back({nx, ny});
        if (cell.piece && cell.piece->kingdom != piece.kingdom) break;
    }
    return moves;
}
