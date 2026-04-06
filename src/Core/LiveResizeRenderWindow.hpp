#pragma once

// SFML caches the window size in WindowBase::m_size and normally refreshes it
// only when a Resized event is filtered through pollEvent(). During the native
// Windows resize loop, we need to refresh that cache immediately.
#define private protected
#include <SFML/Graphics.hpp>
#undef private

class LiveResizeRenderWindow : public sf::RenderWindow {
public:
    using sf::RenderWindow::RenderWindow;

    void forceResizeCache(sf::Vector2u newSize) {
        if (m_size == newSize) {
            return;
        }

        m_size = newSize;
        onResize();
    }
};