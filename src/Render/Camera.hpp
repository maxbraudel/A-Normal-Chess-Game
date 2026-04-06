#pragma once
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

class Camera {
public:
    Camera();
    void init(sf::RenderWindow& window);
    void handleWindowResize(sf::Vector2u newSize);

    void zoom(float factor);
    void pan(sf::Vector2f delta);
    void centerOn(sf::Vector2f worldPos);

    void applyTo(sf::RenderWindow& window) const;
    sf::FloatRect getViewBounds() const;
    sf::Vector2f screenToWorld(sf::Vector2i screenPos, const sf::RenderWindow& window) const;
    sf::Vector2f worldToScreen(sf::Vector2f worldPos, const sf::RenderWindow& window) const;
    sf::Vector2i worldToCell(sf::Vector2f worldPos, int cellSize) const;

    float getZoomLevel() const;

private:
    sf::View m_view;
    float m_zoomLevel;
};
