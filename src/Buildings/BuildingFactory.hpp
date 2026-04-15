#pragma once
#include "Buildings/Building.hpp"

class GameConfig;

class BuildingFactory {
public:
    BuildingFactory();
    Building createBuilding(BuildingType type, KingdomId owner, sf::Vector2i origin,
                            const GameConfig& config, int rotationQuarterTurns = 0);
    void reset(int nextId = 100);
    void observeExisting(int existingId);
private:
    int m_nextId;
};
