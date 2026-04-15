#pragma once
#include <SFML/System/Vector2.hpp>
#include "Buildings/BuildingType.hpp"
#include "Kingdom/KingdomId.hpp"

class Piece;
class Board;
class Kingdom;
class GameConfig;
class Building;

class BuildSystem {
public:
    static bool canBuild(BuildingType type, sf::Vector2i origin,
                         const Board& board,
                         const Kingdom& kingdom, const GameConfig& config,
                         int rotationQuarterTurns = 0);
    static Building place(BuildingType type, sf::Vector2i origin,
                           KingdomId owner, Board& board, const GameConfig& config,
                           int rotationQuarterTurns = 0);
};
