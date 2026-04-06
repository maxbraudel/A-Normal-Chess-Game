#pragma once
#include "Buildings/Building.hpp"

class GameConfig;

class BuildingFactory {
public:
    BuildingFactory();
    Building createBuilding(BuildingType type, KingdomId owner, sf::Vector2i origin,
                            const GameConfig& config);
private:
    int m_nextId;
};
