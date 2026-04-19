#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <set>
#include <vector>

#include "Autonomous/AutonomousUnit.hpp"
#include "Board/Cell.hpp"
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
    void drawTacticalGridBlockedTerrain(sf::RenderWindow& window,
                                        const Camera& camera,
                                        const Board& board);
    void drawTacticalGridBlockedStructures(sf::RenderWindow& window,
                                           const Camera& camera,
                                           const Board& board,
                                           KingdomId localPerspective,
                                           const WeatherMaskCache& weatherMaskCache);
    void drawOccludableBuildings(sf::RenderWindow& window,
                                 const Camera& camera,
                                 const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                 KingdomId localPerspective,
                                 const WeatherMaskCache& weatherMaskCache);
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
                              KingdomId localPerspective,
                              const WeatherMaskCache& weatherMaskCache);
    void drawVisiblePieces(sf::RenderWindow& window,
                           const Camera& camera,
                           const std::array<Kingdom, kNumKingdoms>& kingdoms,
                           KingdomId localPerspective);
    void drawAutonomousUnitsLayer(sf::RenderWindow& window,
                                  const Camera& camera,
                                  const std::vector<AutonomousUnit>& autonomousUnits,
                                  const WeatherMaskCache& weatherMaskCache);
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
    void drawBuildingsByOcclusion(sf::RenderWindow& window,
                                  const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                  KingdomId localPerspective,
                                  bool drawOccludable,
                                  const WeatherMaskCache* weatherMaskCache = nullptr);
    void drawPiecesByOcclusion(sf::RenderWindow& window,
                               const std::array<Kingdom, kNumKingdoms>& kingdoms,
                               KingdomId localPerspective,
                               bool drawOccludable,
                               const WeatherMaskCache* weatherMaskCache = nullptr);
    void drawSingleBuilding(sf::RenderWindow& window,
                            const Building& building,
                            const WeatherMaskCache* weatherMaskCache = nullptr,
                            KingdomId localPerspective = KingdomId::White);
    void drawSingleMapObject(sf::RenderWindow& window, const MapObject& mapObject);
    void drawAutonomousUnits(sf::RenderWindow& window,
                             const std::vector<AutonomousUnit>& autonomousUnits,
                             const WeatherMaskCache* weatherMaskCache = nullptr);

    const AssetManager* m_assets;
    int m_cellSize;
    std::set<int> m_skipPieceIds;
    OverlayRenderer m_overlay;
};
