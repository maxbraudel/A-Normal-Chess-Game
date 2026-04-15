#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "Buildings/BuildingType.hpp"
#include "Render/StructureOverlay.hpp"

class AssetManager;
class Camera;
class Building;
class Board;

class OverlayRenderer {
public:
    void drawOrientationCheckerboard(sf::RenderWindow& window, const Board& board, int cellSize);
    void drawSelectionFrame(sf::RenderWindow& window, const Camera& camera,
                             const sf::View& hudView, sf::Vector2u windowSize,
                             sf::Vector2i origin, int width, int height, int cellSize);
    void drawReachableCells(sf::RenderWindow& window, const Camera& camera,
                             const std::vector<sf::Vector2i>& cells, int cellSize);
    void drawOriginCell(sf::RenderWindow& window, const Camera& camera,
                         sf::Vector2i origin, int cellSize);
    void drawDangerCells(sf::RenderWindow& window, const Camera& camera,
                          const std::vector<sf::Vector2i>& cells, int cellSize);
    void drawBuildPreview(sf::RenderWindow& window, const Camera& camera,
                           sf::Vector2i origin, BuildingType type,
                           int width, int height,
                           int rotationQuarterTurns, int flipMask,
                           int cellSize, bool valid,
                           const AssetManager& assets);
    void drawStructureOverlay(sf::RenderWindow& window, const Camera& camera,
                               const sf::View& hudView, sf::Vector2u windowSize,
                               const Building& building, const StructureOverlayStack& overlay,
                               int cellSize, const AssetManager& assets);
};
