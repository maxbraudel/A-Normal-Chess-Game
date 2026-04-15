#include "Buildings/BuildingFactory.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/StructureIntegrityRules.hpp"

BuildingFactory::BuildingFactory() : m_nextId(100) {}

Building BuildingFactory::createBuilding(BuildingType type, KingdomId owner, sf::Vector2i origin,
                                          const GameConfig& config, int rotationQuarterTurns) {
    Building b;
    b.id = m_nextId++;
    b.type = type;
    b.owner = owner;
    b.isNeutral = false;
    b.origin = origin;
    b.width = config.getBuildingWidth(type);
    b.height = config.getBuildingHeight(type);
    b.rotationQuarterTurns = rotationQuarterTurns;
    b.flipMask = 0;
    b.isProducing = false;
    b.producingType = 0;
    b.turnsRemaining = 0;

    const int hp = StructureIntegrityRules::defaultCellHP(type, config);
    b.cellHP.assign(b.width * b.height, hp);
    b.cellBreachState.assign(b.width * b.height, 0);
    return b;
}

void BuildingFactory::reset(int nextId) {
    m_nextId = nextId;
}

void BuildingFactory::observeExisting(int existingId) {
    if (existingId >= m_nextId) {
        m_nextId = existingId + 1;
    }
}
