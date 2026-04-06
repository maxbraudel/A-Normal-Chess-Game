#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <vector>
#include "Kingdom/KingdomId.hpp"

class AssetManager;
class Camera;
class Board;
class Kingdom;
class Building;
class TurnSystem;

class OverlayRenderer {
public:
    void drawSelectedPieceMarker(sf::RenderWindow& window, const Camera& camera,
                                  sf::Vector2i piecePos, int cellSize);
    void drawReachableCells(sf::RenderWindow& window, const Camera& camera,
                             const std::vector<sf::Vector2i>& cells, int cellSize);
    void drawOriginCell(sf::RenderWindow& window, const Camera& camera,
                         sf::Vector2i origin, int cellSize);
    void drawDangerCells(sf::RenderWindow& window, const Camera& camera,
                          const std::vector<sf::Vector2i>& cells, int cellSize);
    void drawBuildPreview(sf::RenderWindow& window, const Camera& camera,
                           sf::Vector2i origin, int width, int height, int cellSize, bool valid);
    void drawZoneIndicators(sf::RenderWindow& window, const Camera& camera,
                             const Board& board, const std::vector<Building>& publicBuildings,
                             const std::array<Kingdom, kNumKingdoms>& kingdoms,
                             int cellSize, const AssetManager& assets);
};
