#include "Core/GameClock.hpp"

GameClock::GameClock() : m_deltaTime(0.f) {}

void GameClock::update() {
    m_deltaTime = m_clock.restart().asSeconds();
}

float GameClock::restart() {
    m_deltaTime = m_clock.restart().asSeconds();
    return m_deltaTime;
}

float GameClock::getDeltaTime() const {
    return m_deltaTime;
}
