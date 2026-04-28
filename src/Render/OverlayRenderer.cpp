#include "Render/OverlayRenderer.hpp"
#include "Render/Camera.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Assets/AssetManager.hpp"
#include <algorithm>
#include <cmath>

namespace {

constexpr int kFlipHorizontalMask = 1;
constexpr int kFlipVerticalMask = 2;
constexpr float kPi = 3.14159265358979323846f;

void drawDot(sf::RenderTarget& target, float x, float y, float diameter, const sf::Color& color) {
    sf::RectangleShape dot({diameter, diameter});
    dot.setFillColor(color);
    dot.setPosition(x, y);
    target.draw(dot);
}

void drawHorizontalDotRow(sf::RenderTarget& target, float startX, float endX,
                          float y, float diameter, float gapLength, const sf::Color& color) {
    if (endX < startX) {
        return;
    }

    const float step = diameter + gapLength;
    float lastX = startX;
    for (float x = startX; x <= endX; x += step) {
        drawDot(target, x, y, diameter, color);
        lastX = x;
    }

    if (endX - lastX > 0.5f) {
        drawDot(target, endX, y, diameter, color);
    }
}

void drawVerticalDotColumn(sf::RenderTarget& target, float x, float startY, float endY,
                           float diameter, float gapLength, const sf::Color& color) {
    if (endY < startY) {
        return;
    }

    const float step = diameter + gapLength;
    float lastY = startY;
    for (float y = startY; y <= endY; y += step) {
        drawDot(target, x, y, diameter, color);
        lastY = y;
    }

    if (endY - lastY > 0.5f) {
        drawDot(target, x, endY, diameter, color);
    }
}

sf::Vector2f lerpPoint(const sf::Vector2f& start, const sf::Vector2f& end, float t) {
    return sf::Vector2f{
        start.x + ((end.x - start.x) * t),
        start.y + ((end.y - start.y) * t)
    };
}

void drawSolidSegment(sf::RenderTarget& target,
                      const sf::Vector2f& start,
                      const sf::Vector2f& end,
                      float thickness,
                      const sf::Color& color) {
    const sf::Vector2f delta{end.x - start.x, end.y - start.y};
    const float length = std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
    if (length <= 0.5f) {
        return;
    }

    sf::RectangleShape line({length, thickness});
    line.setFillColor(color);
    line.setOrigin(0.f, thickness * 0.5f);
    line.setPosition(start);
    line.setRotation(std::atan2(delta.y, delta.x) * 180.f / kPi);
    target.draw(line);
}

void drawDottedSegment(sf::RenderTarget& target,
                       const sf::Vector2f& start,
                       const sf::Vector2f& end,
                       float diameter,
                       float gapLength,
                       const sf::Color& color) {
    const sf::Vector2f delta{end.x - start.x, end.y - start.y};
    const float length = std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
    if (length <= 0.5f) {
        return;
    }

    const sf::Vector2f direction{delta.x / length, delta.y / length};
    const float step = diameter + gapLength;
    float lastDistance = 0.f;
    for (float distance = 0.f; distance <= length; distance += step) {
        const sf::Vector2f point{
            start.x + (direction.x * distance),
            start.y + (direction.y * distance)};
        drawDot(target,
                point.x - (diameter * 0.5f),
                point.y - (diameter * 0.5f),
                diameter,
                color);
        lastDistance = distance;
    }

    if (length - lastDistance > 0.5f) {
        drawDot(target,
                end.x - (diameter * 0.5f),
                end.y - (diameter * 0.5f),
                diameter,
                color);
    }
}

void drawDottedFrame(sf::RenderWindow& window, const Camera& camera,
                     const sf::View& hudView, sf::Vector2u windowSize,
                     sf::Vector2i origin, int width, int height, int cellSize,
                     const sf::Color& color) {
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
    drawHorizontalDotRow(window, left, right, top, dotDiameter, gapLength, color);
    drawHorizontalDotRow(window, left, right, bottom, dotDiameter, gapLength, color);
    drawVerticalDotColumn(window, left, top, bottom, dotDiameter, gapLength, color);
    drawVerticalDotColumn(window, right, top, bottom, dotDiameter, gapLength, color);
    window.setView(savedView);
}

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

const sf::Texture& getOverlayTexture(const StructureOverlayIcon& icon,
                                     const AssetManager& assets) {
    if (icon.source == StructureOverlayIconSource::PieceTexture) {
        return assets.getPieceTexture(icon.pieceType, icon.kingdom);
    }

    return assets.getUITexture(icon.textureName);
}

float measureTextWidth(const std::string& text,
                       const AssetManager& assets,
                       int textSizePx) {
    if (text.empty()) {
        return 0.f;
    }

    if (!assets.hasFont()) {
        return static_cast<float>(text.size()) * static_cast<float>(textSizePx) * 0.6f;
    }

    sf::Text drawable;
    drawable.setFont(assets.getFont());
    drawable.setCharacterSize(static_cast<unsigned int>(textSizePx));
    drawable.setString(text);
    const sf::FloatRect bounds = drawable.getLocalBounds();
    return bounds.width;
}

float measureTextHeight(const std::string& text,
                        const AssetManager& assets,
                        int textSizePx) {
    if (text.empty()) {
        return 0.f;
    }

    if (!assets.hasFont()) {
        return static_cast<float>(textSizePx);
    }

    sf::Text drawable;
    drawable.setFont(assets.getFont());
    drawable.setCharacterSize(static_cast<unsigned int>(textSizePx));
    drawable.setString(text);
    const sf::FloatRect bounds = drawable.getLocalBounds();
    return bounds.height;
}

float measureOverlayItemWidth(const StructureOverlayItem& item,
                              const AssetManager& assets,
                              const StructureOverlayRenderingConfig& style) {
    switch (item.type) {
        case StructureOverlayItemType::Icon:
            return static_cast<float>(style.iconSizePx);
        case StructureOverlayItemType::Text:
            return measureTextWidth(item.text, assets, style.textSizePx);
        case StructureOverlayItemType::ProgressBar:
            return static_cast<float>(style.progressWidthPx);
    }

    return 0.f;
}

float measureOverlayItemHeight(const StructureOverlayItem& item,
                               const AssetManager& assets,
                               const StructureOverlayRenderingConfig& style) {
    switch (item.type) {
        case StructureOverlayItemType::Icon:
            return static_cast<float>(style.iconSizePx);
        case StructureOverlayItemType::Text:
            return measureTextHeight(item.text, assets, style.textSizePx);
        case StructureOverlayItemType::ProgressBar:
            return static_cast<float>(style.progressHeightPx);
    }

    return 0.f;
}

float measureOverlayRowWidth(const StructureOverlayRow& row,
                             const AssetManager& assets,
                             const StructureOverlayRenderingConfig& style) {
    float totalWidth = 0.f;
    for (std::size_t index = 0; index < row.items.size(); ++index) {
        totalWidth += measureOverlayItemWidth(row.items[index], assets, style);
        if (index + 1 < row.items.size()) {
            totalWidth += static_cast<float>(style.itemGapPx);
        }
    }

    return totalWidth;
}

float measureOverlayRowHeight(const StructureOverlayRow& row,
                              const AssetManager& assets,
                              const StructureOverlayRenderingConfig& style) {
    float rowHeight = 0.f;
    for (const StructureOverlayItem& item : row.items) {
        rowHeight = std::max(rowHeight, measureOverlayItemHeight(item, assets, style));
    }

    return rowHeight;
}

float measureOverlayGroupHeight(const std::vector<const StructureOverlayRow*>& rows,
                                const AssetManager& assets,
                                const StructureOverlayRenderingConfig& style) {
    float totalHeight = 0.f;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        totalHeight += measureOverlayRowHeight(*rows[index], assets, style);
        if (index + 1 < rows.size()) {
            totalHeight += static_cast<float>(style.rowGapPx);
        }
    }

    return totalHeight;
}

void drawCenteredText(sf::RenderWindow& window, const AssetManager& assets,
                      const std::string& text, const sf::FloatRect& bounds,
                      const sf::Color& color,
                      int textSizePx) {
    if (text.empty() || !assets.hasFont()) {
        return;
    }

    sf::Text drawable;
    drawable.setFont(assets.getFont());
    drawable.setCharacterSize(static_cast<unsigned int>(textSizePx));
    drawable.setFillColor(color);
    drawable.setString(text);

    const sf::FloatRect textBounds = drawable.getLocalBounds();
    drawable.setPosition(
        bounds.left + (bounds.width - textBounds.width) * 0.5f - textBounds.left,
        bounds.top + (bounds.height - textBounds.height) * 0.5f - textBounds.top);
    window.draw(drawable);
}

void drawOverlayRow(sf::RenderWindow& window, const StructureOverlayRow& row,
                    float rowTop, float centerX,
                    const AssetManager& assets,
                    const StructureOverlayRenderingConfig& style) {
    const float rowWidth = measureOverlayRowWidth(row, assets, style);
    const float rowHeight = measureOverlayRowHeight(row, assets, style);
    float itemLeft = centerX - rowWidth * 0.5f;

    for (std::size_t index = 0; index < row.items.size(); ++index) {
        const StructureOverlayItem& item = row.items[index];
        const float itemWidth = measureOverlayItemWidth(item, assets, style);
        const float itemHeight = measureOverlayItemHeight(item, assets, style);
        const float itemTop = rowTop + (rowHeight - itemHeight) * 0.5f;

        switch (item.type) {
            case StructureOverlayItemType::Icon: {
                sf::Sprite sprite(getOverlayTexture(item.icon, assets));
                const sf::Texture* texture = sprite.getTexture();
                if (texture && texture->getSize().x > 0 && texture->getSize().y > 0) {
                    sprite.setScale(itemWidth / static_cast<float>(texture->getSize().x),
                                    itemHeight / static_cast<float>(texture->getSize().y));
                }
                sprite.setPosition(itemLeft, itemTop);
                window.draw(sprite);
                break;
            }
            case StructureOverlayItemType::Text:
                drawCenteredText(window, assets, item.text,
                                 {itemLeft, rowTop, itemWidth, rowHeight}, style.text, style.textSizePx);
                break;
            case StructureOverlayItemType::ProgressBar: {
                sf::RectangleShape background({itemWidth, itemHeight});
                background.setFillColor(style.background);
                background.setOutlineThickness(1.f);
                background.setOutlineColor(style.outline);
                background.setPosition(itemLeft, itemTop);
                window.draw(background);

                const float clampedProgress = std::clamp(item.progress, 0.f, 1.f);
                if (clampedProgress > 0.f) {
                    sf::RectangleShape fill({itemWidth * clampedProgress, itemHeight});
                    fill.setFillColor(style.fill);
                    fill.setPosition(itemLeft, itemTop);
                    window.draw(fill);
                }

                drawCenteredText(window, assets, item.text,
                                 {itemLeft, itemTop, itemWidth, itemHeight}, style.text, style.textSizePx);
                break;
            }
        }

        itemLeft += itemWidth;
        if (index + 1 < row.items.size()) {
            itemLeft += static_cast<float>(style.itemGapPx);
        }
    }
}

} // namespace

void OverlayRenderer::configure(const RenderingStyleConfig& renderingStyle) {
    m_renderingStyle = renderingStyle;
}

void OverlayRenderer::drawOrientationCheckerboard(sf::RenderWindow& window,
                                                  const Board& board,
                                                  int cellSize) {
    sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(cellSize), static_cast<float>(cellSize)));

    const int diameter = board.getDiameter();
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle) {
                continue;
            }

            overlay.setFillColor(((x + y) % 2 == 0)
                ? m_renderingStyle.orientationCheckerboard.dark
                : m_renderingStyle.orientationCheckerboard.light);
            overlay.setPosition(static_cast<float>(x * cellSize), static_cast<float>(y * cellSize));
            window.draw(overlay);
        }
    }
}

void OverlayRenderer::drawTacticalGridCheckerboard(sf::RenderWindow& window,
                                                   const Board& board,
                                                   int cellSize) {
    sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(cellSize), static_cast<float>(cellSize)));

    const int diameter = board.getDiameter();
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle) {
                continue;
            }

            overlay.setFillColor(((x + y) % 2 == 0)
                ? m_renderingStyle.tacticalGrid.checkerDark
                : m_renderingStyle.tacticalGrid.checkerLight);
            overlay.setPosition(static_cast<float>(x * cellSize), static_cast<float>(y * cellSize));
            window.draw(overlay);
        }
    }
}

void OverlayRenderer::drawSelectionFrame(sf::RenderWindow& window, const Camera& camera,
                                           const sf::View& hudView, sf::Vector2u windowSize,
                                           sf::Vector2i origin, int width, int height, int cellSize) {
    drawDottedFrame(window, camera, hudView, windowSize,
                    origin, width, height, cellSize, m_renderingStyle.selectionOverlay.selectionFrame);
}

void OverlayRenderer::drawReachableCells(sf::RenderWindow& window, const Camera& camera,
                                           const std::vector<sf::Vector2i>& cells, int cellSize) {
    (void)camera;
    sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(cellSize), static_cast<float>(cellSize)));
    overlay.setFillColor(m_renderingStyle.selectionOverlay.reachableCell);

    for (const auto& pos : cells) {
        overlay.setPosition(static_cast<float>(pos.x * cellSize), static_cast<float>(pos.y * cellSize));
        window.draw(overlay);
    }
}

void OverlayRenderer::drawOriginCell(sf::RenderWindow& window, const Camera& camera,
                                       sf::Vector2i origin, int cellSize, sf::Color color) {
    (void)camera;
    sf::RectangleShape rect(sf::Vector2f(static_cast<float>(cellSize), static_cast<float>(cellSize)));
    rect.setFillColor(color);
    rect.setPosition(static_cast<float>(origin.x * cellSize), static_cast<float>(origin.y * cellSize));
    window.draw(rect);
}

void OverlayRenderer::drawDangerCells(sf::RenderWindow& window, const Camera& camera,
                                        const std::vector<sf::Vector2i>& cells, int cellSize) {
    (void)camera;
    sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(cellSize), static_cast<float>(cellSize)));
    overlay.setFillColor(m_renderingStyle.selectionOverlay.dangerCell);
    for (const auto& pos : cells) {
        overlay.setPosition(static_cast<float>(pos.x * cellSize), static_cast<float>(pos.y * cellSize));
        window.draw(overlay);
    }
}

void OverlayRenderer::drawBuildPreview(sf::RenderWindow& window, const Camera& camera,
                                         const sf::View& hudView, sf::Vector2u windowSize,
                                         sf::Vector2i origin, BuildingType type,
                                         int width, int height,
                                         int rotationQuarterTurns, int flipMask,
                                         int cellSize, bool valid,
                                         const AssetManager& assets) {
    const int footprintWidth = Building::getFootprintWidthFor(width, height, rotationQuarterTurns);
    const int footprintHeight = Building::getFootprintHeightFor(width, height, rotationQuarterTurns);
    const sf::Color fillColor = valid
        ? m_renderingStyle.buildPreview.validFill
        : m_renderingStyle.buildPreview.invalidFill;
    const sf::Color outlineColor = valid
        ? m_renderingStyle.buildPreview.validOutline
        : m_renderingStyle.buildPreview.invalidOutline;

    sf::Sprite sprite;
    sprite.setColor(sf::Color(255, 255, 255, 128));
    for (int dy = 0; dy < footprintHeight; ++dy) {
        for (int dx = 0; dx < footprintWidth; ++dx) {
            const sf::Vector2i sourceLocal = Building::mapFootprintToSourceLocalFor(
                dx, dy, width, height, rotationQuarterTurns, flipMask);
            if (sourceLocal.x < 0 || sourceLocal.y < 0) {
                continue;
            }

            sprite.setTexture(assets.getBuildingTexture(type, sourceLocal.x, sourceLocal.y));
            configureSpriteForCell(sprite, cellSize,
                                   static_cast<float>((origin.x + dx) * cellSize),
                                   static_cast<float>((origin.y + dy) * cellSize),
                                   rotationQuarterTurns, flipMask);
            window.draw(sprite);

            sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(cellSize), static_cast<float>(cellSize)));
            overlay.setFillColor(fillColor);
            overlay.setPosition(static_cast<float>((origin.x + dx) * cellSize),
                                static_cast<float>((origin.y + dy) * cellSize));
            window.draw(overlay);
        }
    }

    drawDottedFrame(window, camera, hudView, windowSize,
                    origin, footprintWidth, footprintHeight, cellSize, outlineColor);
}

void OverlayRenderer::drawActionMarker(sf::RenderWindow& window, const Camera& camera,
                                       const sf::View& hudView, sf::Vector2u windowSize,
                                       sf::Vector2i origin, int width, int height,
                                       const std::string& iconName,
                                       int cellSize, const AssetManager& assets) {
    const sf::Vector2f worldTopLeft(
        static_cast<float>(origin.x * cellSize),
        static_cast<float>(origin.y * cellSize));
    const sf::Vector2f worldBottomRight(
        static_cast<float>((origin.x + width) * cellSize),
        static_cast<float>((origin.y + height) * cellSize));

    const sf::Vector2f screenTopLeft = camera.worldToScreen(worldTopLeft, windowSize);
    const sf::Vector2f screenBottomRight = camera.worldToScreen(worldBottomRight, windowSize);
    const float leftEdge = std::min(screenTopLeft.x, screenBottomRight.x);
    const float topEdge = std::min(screenTopLeft.y, screenBottomRight.y);
    const float rightEdge = std::max(screenTopLeft.x, screenBottomRight.x);
    const float centerX = (leftEdge + rightEdge) * 0.5f;

    sf::Sprite sprite(assets.getUITexture(iconName));
    const sf::Texture* texture = sprite.getTexture();
    if (!texture || texture->getSize().x == 0 || texture->getSize().y == 0) {
        return;
    }

    const float actionMarkerSize = static_cast<float>(m_renderingStyle.structureOverlay.actionMarkerSizePx);
    sprite.setScale(actionMarkerSize / static_cast<float>(texture->getSize().x),
                    actionMarkerSize / static_cast<float>(texture->getSize().y));
    sprite.setPosition(centerX - (actionMarkerSize * 0.5f),
                       topEdge - actionMarkerSize - static_cast<float>(m_renderingStyle.structureOverlay.rowGapPx));

    const sf::View savedView = window.getView();
    window.setView(hudView);
    window.draw(sprite);
    window.setView(savedView);
}

void OverlayRenderer::drawMovePath(sf::RenderTarget& target, const Camera& camera,
                                   const sf::View& hudView, sf::Vector2u windowSize,
                                   sf::Vector2i origin, sf::Vector2i destination,
                                   const std::optional<sf::Vector2i>& elbow,
                                   bool dottedFirstSegment,
                                   int cellSize) {
    (void)hudView;
    const auto cellCenterToScreen = [&](sf::Vector2i cell) {
        const sf::Vector2f worldCenter(
            static_cast<float>(cell.x * cellSize) + (static_cast<float>(cellSize) * 0.5f),
            static_cast<float>(cell.y * cellSize) + (static_cast<float>(cellSize) * 0.5f));
        return camera.worldToScreen(worldCenter, windowSize);
    };

    const sf::Vector2f originScreen = cellCenterToScreen(origin);
    const sf::Vector2f destinationScreen = cellCenterToScreen(destination);
    const sf::Color movePathColor = m_renderingStyle.queuedMovePaths.color;
    const float movePathThickness = static_cast<float>(m_renderingStyle.queuedMovePaths.thicknessPx);
    const float movePathDotDiameter = static_cast<float>(m_renderingStyle.queuedMovePaths.dottedDotSizePx);
    const float movePathDotGap = static_cast<float>(m_renderingStyle.queuedMovePaths.dottedGapPx);

    const sf::View savedView = target.getView();
    target.setView(target.getDefaultView());

    if (!elbow.has_value()) {
        drawSolidSegment(target, originScreen, destinationScreen, movePathThickness, movePathColor);
        target.setView(savedView);
        return;
    }

    const sf::Vector2f elbowScreen = cellCenterToScreen(*elbow);
    if (dottedFirstSegment) {
        const sf::Vector2f dottedStart = lerpPoint(originScreen, elbowScreen, 0.25f);
        const sf::Vector2f dottedEnd = lerpPoint(originScreen, elbowScreen, 0.75f);
        drawSolidSegment(target, originScreen, dottedStart, movePathThickness, movePathColor);
        drawDottedSegment(target, dottedStart, dottedEnd, movePathDotDiameter, movePathDotGap, movePathColor);
        drawSolidSegment(target, dottedEnd, elbowScreen, movePathThickness, movePathColor);
    } else {
        drawSolidSegment(target, originScreen, elbowScreen, movePathThickness, movePathColor);
    }

    drawSolidSegment(target, elbowScreen, destinationScreen, movePathThickness, movePathColor);
    target.setView(savedView);
}

void OverlayRenderer::drawStructureOverlay(sf::RenderWindow& window, const Camera& camera,
                                             const sf::View& hudView, sf::Vector2u windowSize,
                                             const Building& building,
                                             const StructureOverlayStack& overlay,
                                             int cellSize, const AssetManager& assets) {
    if (overlay.isEmpty()) {
        return;
    }

    const sf::Vector2f worldTopLeft(
        static_cast<float>(building.origin.x * cellSize),
        static_cast<float>(building.origin.y * cellSize));
    const sf::Vector2f worldBottomRight(
        static_cast<float>((building.origin.x + building.getFootprintWidth()) * cellSize),
        static_cast<float>((building.origin.y + building.getFootprintHeight()) * cellSize));

    const sf::Vector2f screenTopLeft = camera.worldToScreen(worldTopLeft, windowSize);
    const sf::Vector2f screenBottomRight = camera.worldToScreen(worldBottomRight, windowSize);
    const float leftEdge = std::min(screenTopLeft.x, screenBottomRight.x);
    const float topEdge = std::min(screenTopLeft.y, screenBottomRight.y);
    const float rightEdge = std::max(screenTopLeft.x, screenBottomRight.x);
    const float centerX = (leftEdge + rightEdge) * 0.5f;

    std::vector<const StructureOverlayRow*> aboveRows;
    std::vector<const StructureOverlayRow*> belowRows;
    for (const StructureOverlayRow& row : overlay.rows) {
        if (row.items.empty()) {
            continue;
        }

        if (row.placement == StructureOverlayRowPlacement::Above) {
            aboveRows.push_back(&row);
        } else {
            belowRows.push_back(&row);
        }
    }

    if (aboveRows.empty() && belowRows.empty()) {
        return;
    }

    const StructureOverlayRenderingConfig& style = m_renderingStyle.structureOverlay;
    const float groupGap = (!aboveRows.empty() && !belowRows.empty())
        ? static_cast<float>(style.rowGapPx)
        : 0.f;
    const float totalHeight = measureOverlayGroupHeight(aboveRows, assets, style)
        + groupGap
        + measureOverlayGroupHeight(belowRows, assets, style);
    float rowTop = topEdge - static_cast<float>(style.marginAboveBuildingPx) - totalHeight;

    const sf::View savedView = window.getView();
    window.setView(hudView);

    for (std::size_t index = 0; index < aboveRows.size(); ++index) {
        const float rowHeight = measureOverlayRowHeight(*aboveRows[index], assets, style);
        drawOverlayRow(window, *aboveRows[index], rowTop, centerX, assets, style);
        rowTop += rowHeight;
        if (index + 1 < aboveRows.size()) {
            rowTop += static_cast<float>(style.rowGapPx);
        }
    }

    if (!aboveRows.empty() && !belowRows.empty()) {
        rowTop += static_cast<float>(style.rowGapPx);
    }

    for (std::size_t index = 0; index < belowRows.size(); ++index) {
        const float rowHeight = measureOverlayRowHeight(*belowRows[index], assets, style);
        drawOverlayRow(window, *belowRows[index], rowTop, centerX, assets, style);
        rowTop += rowHeight;
        if (index + 1 < belowRows.size()) {
            rowTop += static_cast<float>(style.rowGapPx);
        }
    }

    window.setView(savedView);
}
