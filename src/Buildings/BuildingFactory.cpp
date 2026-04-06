#include "Buildings/BuildingFactory.hpp"
#include "Config/GameConfig.hpp"

BuildingFactory::BuildingFactory() : m_nextId(100) {}

Building BuildingFactory::createBuilding(BuildingType type, KingdomId owner, sf::Vector2i origin,
                                          const GameConfig& config) {
    Building b;
    b.id = m_nextId++;
    b.type = type;
    b.owner = owner;
    b.isNeutral = false;
    b.origin = origin;
    b.width = config.getBuildingWidth(type);
    b.height = config.getBuildingHeight(type);
    b.isProducing = false;
    b.producingType = 0;
    b.turnsRemaining = 0;

    int hp = 1;
    if (type == BuildingType::StoneWall) hp = config.getStoneWallHP();
    else if (type == BuildingType::WoodWall) hp = config.getWoodWallHP();
    else if (type == BuildingType::Barracks) hp = config.getBarracksCellHP();

    b.cellHP.assign(b.width * b.height, hp);
    return b;
}
