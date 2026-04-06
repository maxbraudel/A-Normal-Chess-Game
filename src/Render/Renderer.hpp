#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "Render/OverlayRenderer.hpp"

class AssetManager;
class Camera;
class Board;
class Kingdom;
class Building;
class TurnSystem;

class Renderer {
public:
    Renderer();
    void init(const AssetManager& assets, int cellSize);

    void draw(sf::RenderWindow& window, const Camera& camera,
              const Board& board, const Kingdom& white, const Kingdom& black,
              const std::vector<Building>& publicBuildings,
              const TurnSystem& turnSystem);

    OverlayRenderer& getOverlay();

private:
    void drawBoard(sf::RenderWindow& window, const Camera& camera, const Board& board);
    void drawBuildings(sf::RenderWindow& window, const Camera& camera,
                        const Kingdom& white, const Kingdom& black,
                        const std::vector<Building>& publicBuildings);
    void drawPieces(sf::RenderWindow& window, const Camera& camera,
                     const Kingdom& white, const Kingdom& black);
    void drawSingleBuilding(sf::RenderWindow& window, const Building& building);

    const AssetManager* m_assets;
    int m_cellSize;
    OverlayRenderer m_overlay;
};
