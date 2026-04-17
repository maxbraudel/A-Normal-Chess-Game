#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <set>
#include <vector>

#include "Autonomous/AutonomousUnit.hpp"
#include "Objects/MapObject.hpp"
#include "Render/OverlayRenderer.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Systems/WeatherTypes.hpp"

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
                            const std::vector<AutonomousUnit>& autonomousUnits,
              const TurnSystem& turnSystem);
    void drawWorldBase(sf::RenderWindow& window, const Camera& camera,
                       const Board& board, const std::array<Kingdom, kNumKingdoms>& kingdoms,
                       const std::vector<Building>& publicBuildings,
                                             const std::vector<MapObject>& mapObjects,
                                             const std::vector<AutonomousUnit>& autonomousUnits);
    void drawPiecesLayer(sf::RenderWindow& window, const Camera& camera,
                                                 const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                                 const std::vector<AutonomousUnit>& autonomousUnits);
    void drawTerrainLayer(sf::RenderWindow& window, const Camera& camera, const Board& board);
    void drawOccludableBuildings(sf::RenderWindow& window,
                                 const Camera& camera,
                                 const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                 KingdomId localPerspective);
    void drawVisibleBuildings(sf::RenderWindow& window,
                              const Camera& camera,
                              const std::array<Kingdom, kNumKingdoms>& kingdoms,
                              const std::vector<Building>& publicBuildings,
                              KingdomId localPerspective);
    void drawMapObjectsLayer(sf::RenderWindow& window,
                             const Camera& camera,
                             const std::vector<MapObject>& mapObjects);
    void drawOccludablePieces(sf::RenderWindow& window,
                              const Camera& camera,
                              const std::array<Kingdom, kNumKingdoms>& kingdoms,
                              KingdomId localPerspective);
    void drawVisiblePieces(sf::RenderWindow& window,
                           const Camera& camera,
                           const std::array<Kingdom, kNumKingdoms>& kingdoms,
                           KingdomId localPerspective);
    void drawAutonomousUnitsLayer(sf::RenderWindow& window,
                                  const Camera& camera,
                                  const std::vector<AutonomousUnit>& autonomousUnits);
    void drawWeatherLayer(sf::RenderWindow& window,
                          const Camera& camera,
                          const Board& board,
                          const WeatherMaskCache& weatherMaskCache);

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
    void drawAutonomousUnits(sf::RenderWindow& window, const std::vector<AutonomousUnit>& autonomousUnits);
    void drawBuildingsByOcclusion(sf::RenderWindow& window,
                                  const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                  KingdomId localPerspective,
                                  bool drawOccludable);
    void drawPiecesByOcclusion(sf::RenderWindow& window,
                               const std::array<Kingdom, kNumKingdoms>& kingdoms,
                               KingdomId localPerspective,
                               bool drawOccludable);
    void drawSingleBuilding(sf::RenderWindow& window, const Building& building);
    void drawSingleMapObject(sf::RenderWindow& window, const MapObject& mapObject);

    const AssetManager* m_assets;
    int m_cellSize;
    std::set<int> m_skipPieceIds;
    OverlayRenderer m_overlay;
};
