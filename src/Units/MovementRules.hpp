#pragma once
#include <vector>
#include <SFML/System/Vector2.hpp>

class Piece;
class Board;
class GameConfig;

class MovementRules {
public:
    static std::vector<sf::Vector2i> getValidMoves(
        const Piece& piece, const Board& board, const GameConfig& config);

private:
    static std::vector<sf::Vector2i> getPawnMoves(const Piece& piece, const Board& board);
    static std::vector<sf::Vector2i> getKnightMoves(const Piece& piece, const Board& board, const GameConfig& config);
    static std::vector<sf::Vector2i> getBishopMoves(const Piece& piece, const Board& board, const GameConfig& config);
    static std::vector<sf::Vector2i> getRookMoves(const Piece& piece, const Board& board, const GameConfig& config);
    static std::vector<sf::Vector2i> getQueenMoves(const Piece& piece, const Board& board, const GameConfig& config);
    static std::vector<sf::Vector2i> getKingMoves(const Piece& piece, const Board& board);

    static std::vector<sf::Vector2i> traceDirection(
        const Piece& piece, const Board& board, int dx, int dy, int maxRange);
};
