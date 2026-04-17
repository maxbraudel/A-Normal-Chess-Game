#pragma once

#include <string>

#include <SFML/System/Vector2.hpp>

#include "Objects/MapObjectType.hpp"

enum class ChestRewardType {
    Gold = 0,
    MovementPointsMaxBonus,
    BuildPointsMaxBonus
};

struct ChestReward {
    ChestRewardType type = ChestRewardType::Gold;
    int amount = 0;
};

struct ChestData {
    ChestReward reward{};
    int spawnTurn = 0;
};

struct MapObject {
    int id = -1;
    MapObjectType type = MapObjectType::Chest;
    sf::Vector2i position{0, 0};
    ChestData chest{};

    bool blocksMovement() const { return false; }
};

inline const char* mapObjectTypeLabel(MapObjectType type) {
    switch (type) {
        case MapObjectType::Chest:
            return "Chest";
    }

    return "Object";
}

inline const char* chestRewardTypeLabel(ChestRewardType type) {
    switch (type) {
        case ChestRewardType::Gold:
            return "Gold";
        case ChestRewardType::MovementPointsMaxBonus:
            return "Movement Points";
        case ChestRewardType::BuildPointsMaxBonus:
            return "Build Points";
    }

    return "Reward";
}

inline std::string describeChestReward(const ChestReward& reward) {
    switch (reward.type) {
        case ChestRewardType::Gold:
            return "+" + std::to_string(reward.amount) + " gold";
        case ChestRewardType::MovementPointsMaxBonus:
            return "+" + std::to_string(reward.amount)
                + " max movement point"
                + (reward.amount == 1 ? std::string{} : std::string{"s"})
                + " per turn";
        case ChestRewardType::BuildPointsMaxBonus:
            return "+" + std::to_string(reward.amount)
                + " max build point"
                + (reward.amount == 1 ? std::string{} : std::string{"s"})
                + " per turn";
    }

    return "Unknown reward";
}