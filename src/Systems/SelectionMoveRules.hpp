#pragma once

#include <vector>

#include <SFML/System/Vector2.hpp>

#include "Systems/TurnCommand.hpp"

class Board;
class Building;
class GameConfig;
class Kingdom;

struct SelectionMoveOptions {
    std::vector<sf::Vector2i> safeMoves;
    std::vector<sf::Vector2i> unsafeMoves;
    bool originUnsafe = false;

    bool contains(sf::Vector2i destination) const;
};

class SelectionMoveRules {
public:
    static SelectionMoveOptions classifyPieceMoves(const Board& board,
                                                   const Kingdom& activeKingdom,
                                                   const Kingdom& enemyKingdom,
                                                   const std::vector<Building>& publicBuildings,
                                                   int turnNumber,
                                                   const std::vector<TurnCommand>& pendingCommands,
                                                   int pieceId,
                                                   const GameConfig& config);
};