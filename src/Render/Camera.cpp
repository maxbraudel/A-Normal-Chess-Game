#include "Render/Camera.hpp"
#include <algorithm>

Camera::Camera() : m_zoomLevel(1.0f) {}

void Camera::init(sf::RenderWindow& window) {
    m_view = window.getDefaultView();
    m_zoomLevel = 1.0f;
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

sf::Vector2i Camera::worldToCell(sf::Vector2f worldPos, int cellSize) const {
    int cx = static_cast<int>(worldPos.x) / cellSize;
    int cy = static_cast<int>(worldPos.y) / cellSize;
    if (worldPos.x < 0) --cx;
    if (worldPos.y < 0) --cy;
    return {cx, cy};
}

float Camera::getZoomLevel() const { return m_zoomLevel; }
