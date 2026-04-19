#include "Render/OverlayRenderer.hpp"
#include "Render/Camera.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Assets/AssetManager.hpp"
#include <algorithm>

namespace {

const sf::Color kSelectionBlue(80, 160, 255, 240);
const sf::Color kReachableCellColor(0, 255, 0, 80);
const sf::Color kDangerCellColor(255, 40, 40, 90);
const sf::Color kBuildPreviewValidFill(36, 196, 84, 88);
const sf::Color kBuildPreviewInvalidFill(214, 52, 52, 96);
const sf::Color kBuildPreviewValidOutline(92, 255, 144, 196);
const sf::Color kBuildPreviewInvalidOutline(255, 120, 120, 208);
const sf::Color kOverlayBackground(24, 24, 28, 220);
const sf::Color kOverlayOutline(235, 235, 235, 210);
const sf::Color kOverlayFill(80, 160, 255, 220);
const sf::Color kOverlayText(248, 248, 248, 240);
constexpr int kFlipHorizontalMask = 1;
constexpr int kFlipVerticalMask = 2;
constexpr float kOverlayIconSize = 28.f;
constexpr float kActionMarkerSize = 24.f;
constexpr float kOverlayProgressWidth = 96.f;
constexpr float kOverlayProgressHeight = 18.f;
constexpr float kOverlayItemGap = 6.f;
constexpr float kOverlayRowGap = 4.f;
constexpr float kOverlayMarginAboveBuilding = 8.f;
constexpr unsigned int kOverlayTextSize = 13;
const sf::Color kOrientationCheckerDarkColor(36, 36, 36, 96);
const sf::Color kOrientationCheckerLightColor(255, 255, 255, 48);
const sf::Color kTacticalGridCheckerDarkColor(156, 156, 156, 255);
const sf::Color kTacticalGridCheckerLightColor(236, 236, 236, 255);

void drawDot(sf::RenderWindow& window, float x, float y, float diameter, const sf::Color& color) {
    sf::RectangleShape dot({diameter, diameter});
    dot.setFillColor(color);
    dot.setPosition(x, y);
    window.draw(dot);
}

void drawHorizontalDotRow(sf::RenderWindow& window, float startX, float endX,
                          float y, float diameter, float gapLength, const sf::Color& color) {
    if (endX < startX) {
        return;
    }

    const float step = diameter + gapLength;
    float lastX = startX;
    for (float x = startX; x <= endX; x += step) {
        drawDot(window, x, y, diameter, color);
        lastX = x;
    }

    if (endX - lastX > 0.5f) {
        drawDot(window, endX, y, diameter, color);
    }
}

void drawVerticalDotColumn(sf::RenderWindow& window, float x, float startY, float endY,
                           float diameter, float gapLength, const sf::Color& color) {
    if (endY < startY) {
        return;
    }

    const float step = diameter + gapLength;
    float lastY = startY;
    for (float y = startY; y <= endY; y += step) {
        drawDot(window, x, y, diameter, color);
        lastY = y;
    }

    if (endY - lastY > 0.5f) {
        drawDot(window, x, endY, diameter, color);
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

float measureTextWidth(const std::string& text, const AssetManager& assets) {
    if (text.empty()) {
        return 0.f;
    }

    if (!assets.hasFont()) {
        return static_cast<float>(text.size()) * static_cast<float>(kOverlayTextSize) * 0.6f;
    }

    sf::Text drawable;
    drawable.setFont(assets.getFont());
    drawable.setCharacterSize(kOverlayTextSize);
    drawable.setString(text);
    const sf::FloatRect bounds = drawable.getLocalBounds();
    return bounds.width;
}

float measureTextHeight(const std::string& text, const AssetManager& assets) {
    if (text.empty()) {
        return 0.f;
    }

    if (!assets.hasFont()) {
        return static_cast<float>(kOverlayTextSize);
    }

    sf::Text drawable;
    drawable.setFont(assets.getFont());
    drawable.setCharacterSize(kOverlayTextSize);
    drawable.setString(text);
    const sf::FloatRect bounds = drawable.getLocalBounds();
    return bounds.height;
}

float measureOverlayItemWidth(const StructureOverlayItem& item,
                              const AssetManager& assets) {
    switch (item.type) {
        case StructureOverlayItemType::Icon:
            return kOverlayIconSize;
        case StructureOverlayItemType::Text:
            return measureTextWidth(item.text, assets);
        case StructureOverlayItemType::ProgressBar:
            return kOverlayProgressWidth;
    }

    return 0.f;
}

float measureOverlayItemHeight(const StructureOverlayItem& item,
                               const AssetManager& assets) {
    switch (item.type) {
        case StructureOverlayItemType::Icon:
            return kOverlayIconSize;
        case StructureOverlayItemType::Text:
            return measureTextHeight(item.text, assets);
        case StructureOverlayItemType::ProgressBar:
            return kOverlayProgressHeight;
    }

    return 0.f;
}

float measureOverlayRowWidth(const StructureOverlayRow& row,
                             const AssetManager& assets) {
    float totalWidth = 0.f;
    for (std::size_t index = 0; index < row.items.size(); ++index) {
        totalWidth += measureOverlayItemWidth(row.items[index], assets);
        if (index + 1 < row.items.size()) {
            totalWidth += kOverlayItemGap;
        }
    }

    return totalWidth;
}

float measureOverlayRowHeight(const StructureOverlayRow& row,
                              const AssetManager& assets) {
    float rowHeight = 0.f;
    for (const StructureOverlayItem& item : row.items) {
        rowHeight = std::max(rowHeight, measureOverlayItemHeight(item, assets));
    }

    return rowHeight;
}

float measureOverlayGroupHeight(const std::vector<const StructureOverlayRow*>& rows,
                                const AssetManager& assets) {
    float totalHeight = 0.f;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        totalHeight += measureOverlayRowHeight(*rows[index], assets);
        if (index + 1 < rows.size()) {
            totalHeight += kOverlayRowGap;
        }
    }

    return totalHeight;
}

void drawCenteredText(sf::RenderWindow& window, const AssetManager& assets,
                      const std::string& text, const sf::FloatRect& bounds,
                      const sf::Color& color) {
    if (text.empty() || !assets.hasFont()) {
        return;
    }

    sf::Text drawable;
    drawable.setFont(assets.getFont());
    drawable.setCharacterSize(kOverlayTextSize);
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
                    const AssetManager& assets) {
    const float rowWidth = measureOverlayRowWidth(row, assets);
    const float rowHeight = measureOverlayRowHeight(row, assets);
    float itemLeft = centerX - rowWidth * 0.5f;

    for (std::size_t index = 0; index < row.items.size(); ++index) {
        const StructureOverlayItem& item = row.items[index];
        const float itemWidth = measureOverlayItemWidth(item, assets);
        const float itemHeight = measureOverlayItemHeight(item, assets);
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
                                 {itemLeft, rowTop, itemWidth, rowHeight}, kOverlayText);
                break;
            case StructureOverlayItemType::ProgressBar: {
                sf::RectangleShape background({itemWidth, itemHeight});
                background.setFillColor(kOverlayBackground);
                background.setOutlineThickness(1.f);
                background.setOutlineColor(kOverlayOutline);
                background.setPosition(itemLeft, itemTop);
                window.draw(background);

                const float clampedProgress = std::clamp(item.progress, 0.f, 1.f);
                if (clampedProgress > 0.f) {
                    sf::RectangleShape fill({itemWidth * clampedProgress, itemHeight});
                    fill.setFillColor(kOverlayFill);
                    fill.setPosition(itemLeft, itemTop);
                    window.draw(fill);
                }

                drawCenteredText(window, assets, item.text,
                                 {itemLeft, itemTop, itemWidth, itemHeight}, kOverlayText);
                break;
            }
        }

        itemLeft += itemWidth;
        if (index + 1 < row.items.size()) {
            itemLeft += kOverlayItemGap;
        }
    }
}

} // namespace

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
                ? kOrientationCheckerDarkColor
                : kOrientationCheckerLightColor);
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
                ? kTacticalGridCheckerDarkColor
                : kTacticalGridCheckerLightColor);
            overlay.setPosition(static_cast<float>(x * cellSize), static_cast<float>(y * cellSize));
            window.draw(overlay);
        }
    }
}

void OverlayRenderer::drawSelectionFrame(sf::RenderWindow& window, const Camera& camera,
                                           const sf::View& hudView, sf::Vector2u windowSize,
                                           sf::Vector2i origin, int width, int height, int cellSize) {
    drawDottedFrame(window, camera, hudView, windowSize,
                    origin, width, height, cellSize, kSelectionBlue);
}

void OverlayRenderer::drawReachableCells(sf::RenderWindow& window, const Camera& camera,
                                           const std::vector<sf::Vector2i>& cells, int cellSize) {
    (void)camera;
    sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(cellSize), static_cast<float>(cellSize)));
    overlay.setFillColor(kReachableCellColor);

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
    overlay.setFillColor(kDangerCellColor);
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
    const sf::Color fillColor = valid ? kBuildPreviewValidFill : kBuildPreviewInvalidFill;
    const sf::Color outlineColor = valid ? kBuildPreviewValidOutline : kBuildPreviewInvalidOutline;

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

    sprite.setScale(kActionMarkerSize / static_cast<float>(texture->getSize().x),
                    kActionMarkerSize / static_cast<float>(texture->getSize().y));
    sprite.setPosition(centerX - (kActionMarkerSize * 0.5f),
                       topEdge - kActionMarkerSize - 4.f);

    const sf::View savedView = window.getView();
    window.setView(hudView);
    window.draw(sprite);
    window.setView(savedView);
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

    const float groupGap = (!aboveRows.empty() && !belowRows.empty()) ? kOverlayRowGap : 0.f;
    const float totalHeight = measureOverlayGroupHeight(aboveRows, assets)
        + groupGap
        + measureOverlayGroupHeight(belowRows, assets);
    float rowTop = topEdge - kOverlayMarginAboveBuilding - totalHeight;

    const sf::View savedView = window.getView();
    window.setView(hudView);

    for (std::size_t index = 0; index < aboveRows.size(); ++index) {
        const float rowHeight = measureOverlayRowHeight(*aboveRows[index], assets);
        drawOverlayRow(window, *aboveRows[index], rowTop, centerX, assets);
        rowTop += rowHeight;
        if (index + 1 < aboveRows.size()) {
            rowTop += kOverlayRowGap;
        }
    }

    if (!aboveRows.empty() && !belowRows.empty()) {
        rowTop += kOverlayRowGap;
    }

    for (std::size_t index = 0; index < belowRows.size(); ++index) {
        const float rowHeight = measureOverlayRowHeight(*belowRows[index], assets);
        drawOverlayRow(window, *belowRows[index], rowTop, centerX, assets);
        rowTop += rowHeight;
        if (index + 1 < belowRows.size()) {
            rowTop += kOverlayRowGap;
        }
    }

    window.setView(savedView);
}
