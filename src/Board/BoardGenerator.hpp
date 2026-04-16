#pragma once
#include <SFML/System/Vector2.hpp>
#include <cstdint>
#include <random>

class Board;
class GameConfig;
class Building;
#include "Buildings/BuildingType.hpp"
#include <vector>

struct GenerationResult {
    sf::Vector2i playerSpawn;
    sf::Vector2i aiSpawn;
};

class BoardGenerator {
public:
    static GenerationResult generate(Board& board, const GameConfig& config,
                                      std::vector<Building>& publicBuildings,
                                      std::uint32_t worldSeed);
    static void applyTerrainVisuals(Board& board, std::uint32_t worldSeed);

private:
    static bool isConnected(const Board& board, sf::Vector2i from, sf::Vector2i to);
    static sf::Vector2i findValidBuildingPos(const Board& board,
                                              const std::vector<Building>& existing,
                                              int width, int height, int minDist,
                                              int avoidLeftX, int avoidRightX,
                                              BuildingType buildingType,
                                              std::mt19937& random);
    static sf::Vector2i findSpawnCell(const Board& board,
                                       int minX, int maxX,
                                       std::mt19937& random);
    static bool canPlaceBuilding(const Board& board, sf::Vector2i origin, int w, int h);
};
