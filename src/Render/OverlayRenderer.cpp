#include "Render/OverlayRenderer.hpp"
#include "Render/Camera.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Assets/AssetManager.hpp"

void OverlayRenderer::drawSelectedPieceMarker(sf::RenderWindow& window, const Camera& camera,
                                                sf::Vector2i piecePos, int cellSize) {
    static constexpr float DOT_RADIUS = 6.f;

    const float worldX = static_cast<float>(piecePos.x * cellSize + cellSize / 2);
    const float worldY = static_cast<float>(piecePos.y * cellSize) - 10.f;
    const sf::Vector2f screenPos = camera.worldToScreen({worldX, worldY}, window);

    sf::CircleShape dot(DOT_RADIUS);
    dot.setFillColor(sf::Color(80, 160, 255, 230));
    dot.setOutlineColor(sf::Color(180, 220, 255, 255));
    dot.setOutlineThickness(2.f);
    dot.setPosition(screenPos.x - DOT_RADIUS, screenPos.y - DOT_RADIUS);

    const sf::View savedView = window.getView();
    const sf::Vector2u windowSize = window.getSize();
    window.setView(sf::View(sf::FloatRect(0.f, 0.f,
        static_cast<float>(windowSize.x), static_cast<float>(windowSize.y))));
    window.draw(dot);
    window.setView(savedView);
}

void OverlayRenderer::drawReachableCells(sf::RenderWindow& window, const Camera& camera,
                                           const std::vector<sf::Vector2i>& cells, int cellSize) {
    sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(cellSize), static_cast<float>(cellSize)));
    overlay.setFillColor(sf::Color(0, 255, 0, 80));

    for (const auto& pos : cells) {
        overlay.setPosition(static_cast<float>(pos.x * cellSize), static_cast<float>(pos.y * cellSize));
        window.draw(overlay);
    }
}

void OverlayRenderer::drawOriginCell(sf::RenderWindow& window, const Camera& camera,
                                       sf::Vector2i origin, int cellSize) {
    sf::RectangleShape rect(sf::Vector2f(static_cast<float>(cellSize), static_cast<float>(cellSize)));
    rect.setFillColor(sf::Color(40, 120, 255, 130));
    rect.setPosition(static_cast<float>(origin.x * cellSize), static_cast<float>(origin.y * cellSize));
    window.draw(rect);
}

void OverlayRenderer::drawDangerCells(sf::RenderWindow& window, const Camera& camera,
                                        const std::vector<sf::Vector2i>& cells, int cellSize) {
    sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(cellSize), static_cast<float>(cellSize)));
    overlay.setFillColor(sf::Color(255, 40, 40, 90));
    for (const auto& pos : cells) {
        overlay.setPosition(static_cast<float>(pos.x * cellSize), static_cast<float>(pos.y * cellSize));
        window.draw(overlay);
    }
}

void OverlayRenderer::drawBuildPreview(sf::RenderWindow& window, const Camera& camera,
                                         sf::Vector2i origin, int width, int height, int cellSize, bool valid) {
    sf::Color color = valid ? sf::Color(0, 200, 0, 80) : sf::Color(200, 0, 0, 80);

    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            sf::RectangleShape rect(sf::Vector2f(static_cast<float>(cellSize), static_cast<float>(cellSize)));
            rect.setFillColor(color);
            rect.setPosition(static_cast<float>((origin.x + dx) * cellSize),
                            static_cast<float>((origin.y + dy) * cellSize));
            window.draw(rect);
        }
    }
}

void OverlayRenderer::drawZoneIndicators(sf::RenderWindow& window, const Camera& camera,
                                           const Board& board, const std::vector<Building>& publicBuildings,
                                           const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                           int cellSize, const AssetManager& assets) {
    // Fixed icon size in screen pixels — independent of camera zoom
    static constexpr float ICON_SIZE = 28.f;

    // Save current (camera) view, we'll need to restore it after drawing in screen space
    sf::View cameraView = window.getView();

    for (const auto& building : publicBuildings) {
        if (building.type != BuildingType::Mine &&
            building.type != BuildingType::Farm &&
            building.type != BuildingType::Church) continue;

        // Track which kingdoms have a piece on this building
        bool present[kNumKingdoms] = {};

        for (const auto& pos : building.getOccupiedCells()) {
            const Cell& cell = board.getCell(pos.x, pos.y);
            if (cell.piece) {
                present[kingdomIndex(cell.piece->kingdom)] = true;
            }
        }

        bool whitePresent = present[kingdomIndex(KingdomId::White)];
        bool blackPresent = present[kingdomIndex(KingdomId::Black)];
        if (!whitePresent && !blackPresent) continue;

        // World-space anchor: horizontally centered above the building, one cell above top edge
        float worldX = static_cast<float>(building.origin.x * cellSize + building.width * cellSize / 2);
        float worldY = static_cast<float>(building.origin.y * cellSize - cellSize / 2);

        // Convert that world point to screen pixels (respects camera pan + zoom)
        sf::Vector2f screenPos = camera.worldToScreen({worldX, worldY}, window);

        // Pick texture
        const sf::Texture* tex = nullptr;
        if (whitePresent && blackPresent)
            tex = &assets.getUITexture("crossed_swords");
        else if (whitePresent)
            tex = &assets.getUITexture("shield_white");
        else
            tex = &assets.getUITexture("shield_black");

        // Build a fixed-size sprite in screen space
        sf::Sprite spr(*tex);
        sf::Vector2u tsz = tex->getSize();
        if (tsz.x > 0 && tsz.y > 0) {
            spr.setScale(ICON_SIZE / static_cast<float>(tsz.x),
                         ICON_SIZE / static_cast<float>(tsz.y));
        }
        // Center the icon on the anchor point
        spr.setPosition(screenPos.x - ICON_SIZE * 0.5f, screenPos.y - ICON_SIZE * 0.5f);

        // Draw in screen space (default view = no zoom scaling)
        const sf::Vector2u windowSize = window.getSize();
        window.setView(sf::View(sf::FloatRect(0.f, 0.f,
            static_cast<float>(windowSize.x), static_cast<float>(windowSize.y))));
        window.draw(spr);
        window.setView(cameraView);  // Restore camera view immediately after
    }
}
