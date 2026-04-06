#include "Render/Camera.hpp"
#include <algorithm>

Camera::Camera() : m_zoomLevel(1.0f) {}

void Camera::init(sf::RenderWindow& window) {
    m_view = window.getDefaultView();
    m_zoomLevel = 1.0f;
}

void Camera::handleWindowResize(sf::Vector2u newSize) {
    if (newSize.x == 0 || newSize.y == 0) {
        return;
    }

    const sf::Vector2f center = m_view.getCenter();
    m_view.setSize(static_cast<float>(newSize.x) * m_zoomLevel,
                   static_cast<float>(newSize.y) * m_zoomLevel);
    m_view.setCenter(center);
}

void Camera::zoom(float factor) {
    m_zoomLevel *= factor;
    m_zoomLevel = std::max(0.1f, std::min(m_zoomLevel, 20.0f));
    m_view.zoom(factor);
}

void Camera::pan(sf::Vector2f delta) {
    m_view.move(delta);
}

void Camera::centerOn(sf::Vector2f worldPos) {
    m_view.setCenter(worldPos);
}

void Camera::applyTo(sf::RenderWindow& window) const {
    window.setView(m_view);
}

sf::FloatRect Camera::getViewBounds() const {
    sf::Vector2f center = m_view.getCenter();
    sf::Vector2f size = m_view.getSize();
    return sf::FloatRect(center.x - size.x / 2.f, center.y - size.y / 2.f, size.x, size.y);
}

sf::Vector2f Camera::screenToWorld(sf::Vector2i screenPos, const sf::RenderWindow& window) const {
    return window.mapPixelToCoords(screenPos, m_view);
}

sf::Vector2f Camera::worldToScreen(sf::Vector2f worldPos, const sf::RenderWindow& window) const {
    sf::Vector2i px = window.mapCoordsToPixel(worldPos, m_view);
    return sf::Vector2f(static_cast<float>(px.x), static_cast<float>(px.y));
}

sf::Vector2f Camera::worldToScreen(sf::Vector2f worldPos, sf::Vector2u windowSize) const {
    if (windowSize.x == 0 || windowSize.y == 0) {
        return {0.f, 0.f};
    }

    const sf::Vector2f center = m_view.getCenter();
    const sf::Vector2f size = m_view.getSize();
    const float normalizedX = (worldPos.x - (center.x - size.x * 0.5f)) / size.x;
    const float normalizedY = (worldPos.y - (center.y - size.y * 0.5f)) / size.y;

    return {
        normalizedX * static_cast<float>(windowSize.x),
        normalizedY * static_cast<float>(windowSize.y)
    };
}

sf::Vector2i Camera::worldToCell(sf::Vector2f worldPos, int cellSize) const {
    int cx = static_cast<int>(worldPos.x) / cellSize;
    int cy = static_cast<int>(worldPos.y) / cellSize;
    if (worldPos.x < 0) --cx;
    if (worldPos.y < 0) --cy;
    return {cx, cy};
}

float Camera::getZoomLevel() const { return m_zoomLevel; }
