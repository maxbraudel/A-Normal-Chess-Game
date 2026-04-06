#pragma once
#include <SFML/System/Vector2.hpp>

class Board;
class GameConfig;
class Building;
#include <vector>

struct GenerationResult {
    sf::Vector2i playerSpawn;
    sf::Vector2i aiSpawn;
};

class BoardGenerator {
public:
    static GenerationResult generate(Board& board, const GameConfig& config,
                                      std::vector<Building>& publicBuildings);

private:
    static void placeDirtBlobs(Board& board, const GameConfig& config);
    static void placeWaterLakes(Board& board, const GameConfig& config);
    static bool isConnected(const Board& board, sf::Vector2i from, sf::Vector2i to);
    static sf::Vector2i findValidBuildingPos(const Board& board,
                                              const std::vector<Building>& existing,
                                              int width, int height, int minDist,
                                              int avoidLeftX, int avoidRightX);
    static bool canPlaceBuilding(const Board& board, sf::Vector2i origin, int w, int h);
};
