#pragma once
#include "Buildings/Building.hpp"

class GameConfig;

class BuildingFactory {
public:
    BuildingFactory();
    int reserveId();
    Building createBuilding(BuildingType type, KingdomId owner, sf::Vector2i origin,
                            const GameConfig& config, int rotationQuarterTurns = 0,
                            int reservedId = -1);
    void reset(int nextId = 100);
    void observeExisting(int existingId);
private:
    int m_nextId;
};
