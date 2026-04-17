#include "Render/Renderer.hpp"
#include "Render/Camera.hpp"
#include "Render/OverlayRenderer.hpp"
#include "Assets/AssetManager.hpp"
#include "Autonomous/AutonomousUnit.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Objects/MapObject.hpp"
#include "Runtime/WeatherVisibility.hpp"
#include "Systems/WeatherSystem.hpp"
#include "Units/Piece.hpp"
#include "Systems/TurnSystem.hpp"
#include <algorithm>

namespace {

constexpr int kFlipHorizontalMask = 1;
constexpr int kFlipVerticalMask = 2;

void configureSpriteForCell(sf::Sprite& sprite, int cellSize,
                            float cellX, float cellY,
                            int rotationQuarterTurns, int flipMask) {
    const sf::Texture* texture = sprite.getTexture();
    if (!texture) {
        return;
    }

    const sf::Vector2u textureSize = texture->getSize();
    if (textureSize.x == 0 || textureSize.y == 0) {
        return;
    }

    sprite.setOrigin(static_cast<float>(textureSize.x) * 0.5f,
                     static_cast<float>(textureSize.y) * 0.5f);
    sprite.setPosition(cellX + (static_cast<float>(cellSize) * 0.5f),
                       cellY + (static_cast<float>(cellSize) * 0.5f));

    float scaleX = static_cast<float>(cellSize) / static_cast<float>(textureSize.x);
    float scaleY = static_cast<float>(cellSize) / static_cast<float>(textureSize.y);
    if ((flipMask & kFlipHorizontalMask) != 0) {
        scaleX = -scaleX;
    }
    if ((flipMask & kFlipVerticalMask) != 0) {
        scaleY = -scaleY;
    }

    sprite.setScale(scaleX, scaleY);

    int normalizedRotation = rotationQuarterTurns;
    if (normalizedRotation < 0) {
        normalizedRotation = 0;
    }
    normalizedRotation %= 4;
    sprite.setRotation(static_cast<float>(normalizedRotation) * 90.f);
}

} // namespace

Renderer::Renderer() : m_assets(nullptr), m_cellSize(16) {}

void Renderer::init(const AssetManager& assets, int cellSize) {
    m_assets = &assets;
    m_cellSize = cellSize;
}

OverlayRenderer& Renderer::getOverlay() { return m_overlay; }

void Renderer::draw(sf::RenderWindow& window, const Camera& camera,
                     const Board& board, const std::array<Kingdom, kNumKingdoms>& kingdoms,
                     const std::vector<Building>& publicBuildings,
                     const std::vector<MapObject>& mapObjects,
                     const std::vector<AutonomousUnit>& autonomousUnits,
                     const TurnSystem& turnSystem) {
    (void)turnSystem;
    drawWorldBase(window, camera, board, kingdoms, publicBuildings, mapObjects, autonomousUnits);
    drawPiecesLayer(window, camera, kingdoms, autonomousUnits);
}

void Renderer::drawWorldBase(sf::RenderWindow& window, const Camera& camera,
                              const Board& board, const std::array<Kingdom, kNumKingdoms>& kingdoms,
                              const std::vector<Building>& publicBuildings,
                              const std::vector<MapObject>& mapObjects,
                              const std::vector<AutonomousUnit>& autonomousUnits) {
    (void) autonomousUnits;
    camera.applyTo(window);
    drawBoard(window, camera, board);
    drawBuildings(window, camera, kingdoms, publicBuildings);
    drawMapObjects(window, mapObjects);
}

void Renderer::drawPiecesLayer(sf::RenderWindow& window, const Camera& camera,
                                const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                const std::vector<AutonomousUnit>& autonomousUnits) {
    camera.applyTo(window);
    drawPieces(window, camera, kingdoms);
    this->drawAutonomousUnits(window, autonomousUnits, nullptr);
}

void Renderer::drawTerrainLayer(sf::RenderWindow& window, const Camera& camera, const Board& board) {
    camera.applyTo(window);
    drawBoard(window, camera, board);
}

void Renderer::drawOccludableBuildings(sf::RenderWindow& window,
                                       const Camera& camera,
                                       const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                       KingdomId localPerspective,
                                       const WeatherMaskCache& weatherMaskCache) {
    camera.applyTo(window);
    drawBuildingsByOcclusion(window, kingdoms, localPerspective, true, &weatherMaskCache);
}

void Renderer::drawVisibleBuildings(sf::RenderWindow& window,
                                    const Camera& camera,
                                    const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                    const std::vector<Building>& publicBuildings,
                                    KingdomId localPerspective) {
    camera.applyTo(window);
    for (const Building& building : publicBuildings) {
        drawSingleBuilding(window, building);
    }
    drawBuildingsByOcclusion(window, kingdoms, localPerspective, false);
}

void Renderer::drawMapObjectsLayer(sf::RenderWindow& window,
                                   const Camera& camera,
                                   const std::vector<MapObject>& mapObjects) {
    camera.applyTo(window);
    drawMapObjects(window, mapObjects);
}

void Renderer::drawOccludablePieces(sf::RenderWindow& window,
                                    const Camera& camera,
                                    const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                    KingdomId localPerspective,
                                    const WeatherMaskCache& weatherMaskCache) {
    camera.applyTo(window);
    drawPiecesByOcclusion(window, kingdoms, localPerspective, true, &weatherMaskCache);
}

void Renderer::drawVisiblePieces(sf::RenderWindow& window,
                                 const Camera& camera,
                                 const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                 KingdomId localPerspective) {
    camera.applyTo(window);
    drawPiecesByOcclusion(window, kingdoms, localPerspective, false);
}

void Renderer::drawAutonomousUnitsLayer(sf::RenderWindow& window,
                                        const Camera& camera,
                                        const std::vector<AutonomousUnit>& autonomousUnits,
                                        const WeatherMaskCache& weatherMaskCache) {
    camera.applyTo(window);
    drawAutonomousUnits(window, autonomousUnits, &weatherMaskCache);
}

void Renderer::drawWeatherLayer(sf::RenderWindow& window,
                                const Camera& camera,
                                const Board& board,
                                const WeatherMaskCache& weatherMaskCache) {
    if (!weatherMaskCache.hasActiveFront) {
        return;
    }

    camera.applyTo(window);
    const sf::FloatRect viewBounds = camera.getViewBounds();
    const int diameter = board.getDiameter();
    const int minCol = std::max(0, static_cast<int>(viewBounds.left / m_cellSize) - 1);
    const int maxCol = std::min(diameter - 1, static_cast<int>((viewBounds.left + viewBounds.width) / m_cellSize) + 1);
    const int minRow = std::max(0, static_cast<int>(viewBounds.top / m_cellSize) - 1);
    const int maxRow = std::min(diameter - 1, static_cast<int>((viewBounds.top + viewBounds.height) / m_cellSize) + 1);

    sf::RectangleShape cellFog(sf::Vector2f(static_cast<float>(m_cellSize) + 1.0f,
                                            static_cast<float>(m_cellSize) + 1.0f));
    for (int y = minRow; y <= maxRow; ++y) {
        for (int x = minCol; x <= maxCol; ++x) {
            const std::uint8_t alpha = WeatherSystem::alphaAtCell(weatherMaskCache, x, y);
            if (alpha == 0) {
                continue;
            }

            const std::uint8_t shade = WeatherSystem::shadeAtCell(weatherMaskCache, x, y);
            cellFog.setPosition(static_cast<float>(x * m_cellSize), static_cast<float>(y * m_cellSize));
            cellFog.setFillColor(sf::Color(shade, shade, shade, alpha));
            window.draw(cellFog);
        }
    }
}

void Renderer::drawBoard(sf::RenderWindow& window, const Camera& camera, const Board& board) {
    if (!m_assets) return;

    sf::FloatRect viewBounds = camera.getViewBounds();
    int diameter = board.getDiameter();

    int minCol = std::max(0, static_cast<int>(viewBounds.left / m_cellSize));
    int maxCol = std::min(diameter - 1, static_cast<int>((viewBounds.left + viewBounds.width) / m_cellSize) + 1);
    int minRow = std::max(0, static_cast<int>(viewBounds.top / m_cellSize));
    int maxRow = std::min(diameter - 1, static_cast<int>((viewBounds.top + viewBounds.height) / m_cellSize) + 1);

    sf::Sprite sprite;

    for (int y = minRow; y <= maxRow; ++y) {
        for (int x = minCol; x <= maxCol; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle) continue;

            sprite.setTexture(m_assets->getCellTexture(cell.type));
            sprite.setColor(sf::Color(cell.terrainBrightness, cell.terrainBrightness, cell.terrainBrightness));
            configureSpriteForCell(sprite, m_cellSize,
                                   static_cast<float>(x * m_cellSize),
                                   static_cast<float>(y * m_cellSize),
                                   0, cell.terrainFlipMask);

            window.draw(sprite);
        }
    }
}

void Renderer::drawBuildings(sf::RenderWindow& window, const Camera& camera,
                               const std::array<Kingdom, kNumKingdoms>& kingdoms,
                               const std::vector<Building>& publicBuildings) {
    if (!m_assets) return;
    (void) camera;

    // Draw public buildings
    for (const auto& b : publicBuildings) {
        drawSingleBuilding(window, b);
    }

    // Draw private buildings
    for (const auto& k : kingdoms) {
        for (const auto& b : k.buildings) {
            drawSingleBuilding(window, b);
        }
    }
}

void Renderer::drawSingleBuilding(sf::RenderWindow& window,
                                  const Building& building,
                                  const WeatherMaskCache* weatherMaskCache,
                                  KingdomId localPerspective) {
    sf::Sprite sprite;
    const int footprintWidth = building.getFootprintWidth();
    const int footprintHeight = building.getFootprintHeight();

    for (int dy = 0; dy < footprintHeight; ++dy) {
        for (int dx = 0; dx < footprintWidth; ++dx) {
            const sf::Vector2i sourceLocal = building.mapFootprintToSourceLocal(dx, dy);
            if (sourceLocal.x < 0 || sourceLocal.y < 0) {
                continue;
            }

            sprite.setTexture(m_assets->getBuildingTexture(building.type, sourceLocal.x, sourceLocal.y));

            int x = building.origin.x + dx;
            int y = building.origin.y + dy;
            if (weatherMaskCache != nullptr
                && WeatherVisibility::shouldHideBuildingCell(building,
                                                            {x, y},
                                                            localPerspective,
                                                            *weatherMaskCache)) {
                continue;
            }

            configureSpriteForCell(sprite, m_cellSize,
                                   static_cast<float>(x * m_cellSize),
                                   static_cast<float>(y * m_cellSize),
                                   building.rotationQuarterTurns, building.flipMask);

            // Gray out destroyed cells
            int hp = building.getCellHP(dx, dy);
            if ((hp <= 0 || building.isCellBreached(dx, dy)) && !building.isPublic()) {
                sprite.setColor(sf::Color(80, 80, 80, 150));
            } else {
                sprite.setColor(sf::Color::White);
            }

            window.draw(sprite);
        }
    }
}

void Renderer::drawMapObjects(sf::RenderWindow& window, const std::vector<MapObject>& mapObjects) {
    if (!m_assets) return;

    for (const MapObject& mapObject : mapObjects) {
        drawSingleMapObject(window, mapObject);
    }
}

void Renderer::drawSingleMapObject(sf::RenderWindow& window, const MapObject& mapObject) {
    sf::Sprite sprite;
    sprite.setTexture(m_assets->getMapObjectTexture(mapObject.type));
    configureSpriteForCell(sprite,
                           m_cellSize,
                           static_cast<float>(mapObject.position.x * m_cellSize),
                           static_cast<float>(mapObject.position.y * m_cellSize),
                           0,
                           0);
    sprite.setColor(sf::Color::White);
    window.draw(sprite);
}

void Renderer::setSkipPieceIds(const std::set<int>& ids) { m_skipPieceIds = ids; }

void Renderer::drawPieces(sf::RenderWindow& window, const Camera& camera,
                            const std::array<Kingdom, kNumKingdoms>& kingdoms) {
    if (!m_assets) return;
    (void) camera;

    for (const auto& k : kingdoms) {
        for (const auto& piece : k.pieces) {
            if (m_skipPieceIds.count(piece.id) != 0) continue;
            sf::Sprite sprite;
            sprite.setTexture(m_assets->getPieceTexture(piece.type, piece.kingdom));
            sprite.setPosition(static_cast<float>(piece.position.x * m_cellSize),
                              static_cast<float>(piece.position.y * m_cellSize));

            sf::Vector2u texSize = sprite.getTexture()->getSize();
            if (texSize.x > 0 && texSize.y > 0) {
                sprite.setScale(
                    static_cast<float>(m_cellSize) / static_cast<float>(texSize.x),
                    static_cast<float>(m_cellSize) / static_cast<float>(texSize.y)
                );
            }

            window.draw(sprite);
        }
    }
}

void Renderer::drawAutonomousUnits(sf::RenderWindow& window,
                                   const std::vector<AutonomousUnit>& autonomousUnits,
                                   const WeatherMaskCache* weatherMaskCache) {
    if (!m_assets) return;

    sf::Sprite sprite;
    for (const AutonomousUnit& unit : autonomousUnits) {
        if (weatherMaskCache != nullptr
            && WeatherVisibility::shouldHideAutonomousUnit(unit, *weatherMaskCache)) {
            continue;
        }

        sprite.setTexture(m_assets->getInfernalPieceTexture(unit.infernal.manifestedPieceType));
        configureSpriteForCell(sprite,
                               m_cellSize,
                               static_cast<float>(unit.position.x * m_cellSize),
                               static_cast<float>(unit.position.y * m_cellSize),
                               0,
                               0);

        if (unit.infernal.phase == InfernalPhase::Returning) {
            sprite.setColor(sf::Color(255, 220, 220, 220));
        } else {
            sprite.setColor(sf::Color::White);
        }

        window.draw(sprite);
    }
}

void Renderer::drawBuildingsByOcclusion(sf::RenderWindow& window,
                                        const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                        KingdomId localPerspective,
                                        bool drawOccludable,
                                        const WeatherMaskCache* weatherMaskCache) {
    if (!m_assets) return;

    for (const Kingdom& kingdom : kingdoms) {
        for (const Building& building : kingdom.buildings) {
            if (WeatherVisibility::isOccludableBuilding(building, localPerspective) != drawOccludable) {
                continue;
            }

            drawSingleBuilding(window,
                               building,
                               drawOccludable ? weatherMaskCache : nullptr,
                               localPerspective);
        }
    }
}

void Renderer::drawPiecesByOcclusion(sf::RenderWindow& window,
                                     const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                     KingdomId localPerspective,
                                     bool drawOccludable,
                                     const WeatherMaskCache* weatherMaskCache) {
    if (!m_assets) return;

    for (const Kingdom& kingdom : kingdoms) {
        for (const Piece& piece : kingdom.pieces) {
            if (m_skipPieceIds.count(piece.id) != 0) {
                continue;
            }
            if (WeatherVisibility::isOccludablePiece(piece, localPerspective) != drawOccludable) {
                continue;
            }
            if (drawOccludable
                && weatherMaskCache != nullptr
                && WeatherVisibility::shouldHidePiece(piece, localPerspective, *weatherMaskCache)) {
                continue;
            }

            sf::Sprite sprite;
            sprite.setTexture(m_assets->getPieceTexture(piece.type, piece.kingdom));
            sprite.setPosition(static_cast<float>(piece.position.x * m_cellSize),
                               static_cast<float>(piece.position.y * m_cellSize));

            const sf::Vector2u texSize = sprite.getTexture()->getSize();
            if (texSize.x > 0 && texSize.y > 0) {
                sprite.setScale(
                    static_cast<float>(m_cellSize) / static_cast<float>(texSize.x),
                    static_cast<float>(m_cellSize) / static_cast<float>(texSize.y));
            }

            window.draw(sprite);
        }
    }
}
