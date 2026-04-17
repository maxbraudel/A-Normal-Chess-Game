#pragma once

#include <string>

#include "Kingdom/KingdomId.hpp"
#include "Objects/MapObject.hpp"

enum class GameplayNotificationKind {
    ChestReward = 0
};

struct GameplayNotification {
    GameplayNotificationKind kind = GameplayNotificationKind::ChestReward;
    KingdomId kingdom = KingdomId::White;
    ChestReward chestReward{};
};

inline std::string gameplayNotificationTitle(const GameplayNotification& notification) {
    switch (notification.kind) {
        case GameplayNotificationKind::ChestReward:
            return "Chest Opened";
    }

    return "Notification";
}

inline std::string gameplayNotificationMessage(const GameplayNotification& notification) {
    const std::string kingdomName = (notification.kingdom == KingdomId::White) ? "White" : "Black";
    switch (notification.kind) {
        case GameplayNotificationKind::ChestReward:
            switch (notification.chestReward.type) {
                case ChestRewardType::Gold:
                    return kingdomName + " gained " + describeChestReward(notification.chestReward) + ".";
                case ChestRewardType::MovementPointsMaxBonus:
                case ChestRewardType::BuildPointsMaxBonus:
                    return kingdomName + " permanently gained " + describeChestReward(notification.chestReward) + ".";
            }
            break;
    }

    return "A gameplay event occurred.";
}