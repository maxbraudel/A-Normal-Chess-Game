#include "Render/Renderer.hpp"
#include "Render/Camera.hpp"
#include "Render/OverlayRenderer.hpp"
#include "Assets/AssetManager.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Units/Piece.hpp"
#include "Systems/TurnSystem.hpp"
#include <algorithm>

Renderer::Renderer() : m_assets(nullptr), m_cellSize(16) {}

void Renderer::init(const AssetManager& assets, int cellSize) {
    m_assets = &assets;
    m_cellSize = cellSize;
}

OverlayRenderer& Renderer::getOverlay() { return m_overlay; }

void Renderer::draw(sf::RenderWindow& window, const Camera& camera,
                     const Board& board, const std::array<Kingdom, kNumKingdoms>& kingdoms,
                     const std::vector<Building>& publicBuildings,
                     const TurnSystem& turnSystem) {
    (void)turnSystem;
    drawWorldBase(window, camera, board, kingdoms, publicBuildings);
    drawPiecesLayer(window, camera, kingdoms);
}

void Renderer::drawWorldBase(sf::RenderWindow& window, const Camera& camera,
                              const Board& board, const std::array<Kingdom, kNumKingdoms>& kingdoms,
                              const std::vector<Building>& publicBuildings) {
    camera.applyTo(window);
    drawBoard(window, camera, board);
    drawBuildings(window, camera, kingdoms, publicBuildings);
}

void Renderer::drawPiecesLayer(sf::RenderWindow& window, const Camera& camera,
                                const std::array<Kingdom, kNumKingdoms>& kingdoms) {
    camera.applyTo(window);
    drawPieces(window, camera, kingdoms);
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
            sprite.setPosition(static_cast<float>(x * m_cellSize), static_cast<float>(y * m_cellSize));

            // Scale texture to cell size
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

void Renderer::drawBuildings(sf::RenderWindow& window, const Camera& camera,
                               const std::array<Kingdom, kNumKingdoms>& kingdoms,
                               const std::vector<Building>& publicBuildings) {
    if (!m_assets) return;

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

void Renderer::drawSingleBuilding(sf::RenderWindow& window, const Building& building) {
    sf::Sprite sprite;

    for (int dy = 0; dy < building.height; ++dy) {
        for (int dx = 0; dx < building.width; ++dx) {
            sprite.setTexture(m_assets->getBuildingTexture(building.type, dx, dy));

            int x = building.origin.x + dx;
            int y = building.origin.y + dy;

            sprite.setPosition(static_cast<float>(x * m_cellSize), static_cast<float>(y * m_cellSize));

            sf::Vector2u texSize = sprite.getTexture()->getSize();
            if (texSize.x > 0 && texSize.y > 0) {
                sprite.setScale(
                    static_cast<float>(m_cellSize) / static_cast<float>(texSize.x),
                    static_cast<float>(m_cellSize) / static_cast<float>(texSize.y)
                );
            }

            // Gray out destroyed cells
            int hp = building.getCellHP(dx, dy);
            if (hp <= 0 && !building.isPublic()) {
                sprite.setColor(sf::Color(80, 80, 80, 150));
            } else {
                sprite.setColor(sf::Color::White);
            }

            window.draw(sprite);
        }
    }
}

void Renderer::setSkipPieceId(int id) { m_skipPieceId = id; }

void Renderer::drawPieces(sf::RenderWindow& window, const Camera& camera,
                            const std::array<Kingdom, kNumKingdoms>& kingdoms) {
    if (!m_assets) return;

    for (const auto& k : kingdoms) {
        for (const auto& piece : k.pieces) {
            if (piece.id == m_skipPieceId) continue;
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
