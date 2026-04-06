#pragma once
#include <SFML/System/Vector2.hpp>
#include "Units/PieceType.hpp"

class Building;
class Board;
class Kingdom;
class GameConfig;

class ProductionSystem {
public:
    static bool canStartProduction(const Building& barracks, PieceType type,
                                    const Kingdom& kingdom, const GameConfig& config);
    static void startProduction(Building& barracks, PieceType type, const GameConfig& config);
    static void advanceProduction(Building& barracks);
    static bool isProductionComplete(const Building& barracks);
    static sf::Vector2i findSpawnCell(const Building& barracks, const Board& board);
};
