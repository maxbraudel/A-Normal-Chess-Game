#include "Render/OverlayRenderer.hpp"
#include "Render/Camera.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Assets/AssetManager.hpp"
#include <algorithm>

namespace {

const sf::Color kSelectionBlue(80, 160, 255, 240);

void drawDot(sf::RenderWindow& window, float x, float y, float diameter) {
    sf::RectangleShape dot({diameter, diameter});
    dot.setFillColor(kSelectionBlue);
    dot.setPosition(x, y);
    window.draw(dot);
}

void drawHorizontalDotRow(sf::RenderWindow& window, float startX, float endX,
                          float y, float diameter, float gapLength) {
    if (endX < startX) {
        return;
    }

    const float step = diameter + gapLength;
    float lastX = startX;
    for (float x = startX; x <= endX; x += step) {
        drawDot(window, x, y, diameter);
        lastX = x;
    }

    if (endX - lastX > 0.5f) {
        drawDot(window, endX, y, diameter);
    }
}

void drawVerticalDotColumn(sf::RenderWindow& window, float x, float startY, float endY,
                           float diameter, float gapLength) {
    if (endY < startY) {
        return;
    }

    const float step = diameter + gapLength;
    float lastY = startY;
    for (float y = startY; y <= endY; y += step) {
        drawDot(window, x, y, diameter);
        lastY = y;
    }

    if (endY - lastY > 0.5f) {
        drawDot(window, x, endY, diameter);
    }
}

} // namespace

void OverlayRenderer::drawSelectionFrame(sf::RenderWindow& window, const Camera& camera,
                                           const sf::View& hudView, sf::Vector2u windowSize,
                                           sf::Vector2i origin, int width, int height, int cellSize) {
    const float dotDiameter = 4.f;
    const float gapLength = 4.f;

    const sf::Vector2f worldTopLeft(
        static_cast<float>(origin.x * cellSize),
        static_cast<float>(origin.y * cellSize));
    const sf::Vector2f worldBottomRight(
        static_cast<float>((origin.x + width) * cellSize),
        static_cast<float>((origin.y + height) * cellSize));

    sf::Vector2f screenTopLeft = camera.worldToScreen(worldTopLeft, windowSize);
    sf::Vector2f screenBottomRight = camera.worldToScreen(worldBottomRight, windowSize);

    const float leftEdge = std::min(screenTopLeft.x, screenBottomRight.x);
    const float topEdge = std::min(screenTopLeft.y, screenBottomRight.y);
    const float rightEdge = std::max(screenTopLeft.x, screenBottomRight.x);
    const float bottomEdge = std::max(screenTopLeft.y, screenBottomRight.y);

    const float left = leftEdge;
    const float top = topEdge;
    const float right = std::max(leftEdge, rightEdge - dotDiameter);
    const float bottom = std::max(topEdge, bottomEdge - dotDiameter);

    const sf::View savedView = window.getView();
    window.setView(hudView);
    drawHorizontalDotRow(window, left, right, top, dotDiameter, gapLength);
    drawHorizontalDotRow(window, left, right, bottom, dotDiameter, gapLength);
    drawVerticalDotColumn(window, left, top, bottom, dotDiameter, gapLength);
    drawVerticalDotColumn(window, right, top, bottom, dotDiameter, gapLength);
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
                                           const sf::View& hudView, sf::Vector2u windowSize,
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
        sf::Vector2f screenPos = camera.worldToScreen({worldX, worldY}, windowSize);

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
        window.setView(hudView);
        window.draw(spr);
        window.setView(cameraView);  // Restore camera view immediately after
    }
}
