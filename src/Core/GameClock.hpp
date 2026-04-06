#pragma once
#include <SFML/System/Clock.hpp>

class GameClock {
public:
    GameClock();
    void update();
    float restart();
    float getDeltaTime() const;
private:
    sf::Clock m_clock;
    float m_deltaTime;
};
