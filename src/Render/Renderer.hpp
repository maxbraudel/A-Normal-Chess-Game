#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <set>
#include <vector>

#include "Objects/MapObject.hpp"
#include "Render/OverlayRenderer.hpp"
#include "Kingdom/KingdomId.hpp"

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
              const Board& board, const std::array<Kingdom, kNumKingdoms>& kingdoms,
              const std::vector<Building>& publicBuildings,
              const std::vector<MapObject>& mapObjects,
              const TurnSystem& turnSystem);
    void drawWorldBase(sf::RenderWindow& window, const Camera& camera,
                       const Board& board, const std::array<Kingdom, kNumKingdoms>& kingdoms,
                       const std::vector<Building>& publicBuildings,
                       const std::vector<MapObject>& mapObjects);
    void drawPiecesLayer(sf::RenderWindow& window, const Camera& camera,
                         const std::array<Kingdom, kNumKingdoms>& kingdoms);

    OverlayRenderer& getOverlay();
    void setSkipPieceIds(const std::set<int>& ids); // hide pieces from rendering (used for capture preview)

private:
    void drawBoard(sf::RenderWindow& window, const Camera& camera, const Board& board);
    void drawBuildings(sf::RenderWindow& window, const Camera& camera,
                        const std::array<Kingdom, kNumKingdoms>& kingdoms,
                        const std::vector<Building>& publicBuildings);
    void drawMapObjects(sf::RenderWindow& window, const std::vector<MapObject>& mapObjects);
    void drawPieces(sf::RenderWindow& window, const Camera& camera,
                    const std::array<Kingdom, kNumKingdoms>& kingdoms);
    void drawSingleBuilding(sf::RenderWindow& window, const Building& building);
    void drawSingleMapObject(sf::RenderWindow& window, const MapObject& mapObject);

    const AssetManager* m_assets;
    int m_cellSize;
    std::set<int> m_skipPieceIds;
    OverlayRenderer m_overlay;
};
