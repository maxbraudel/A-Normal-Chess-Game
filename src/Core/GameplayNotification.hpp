#pragma once

#include <string>

#include "Core/LocalPlayerContext.hpp"
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

inline bool shouldShowGameplayNotificationForLocalPlayer(const GameplayNotification& notification,
                                                         const LocalPlayerContext& localPlayerContext) {
    return localPlayerContext.isLocallyControlled(notification.kingdom);
}

inline std::string gameplayNotificationRecipientLabel(const GameplayNotification& notification,
                                                      const LocalPlayerContext& localPlayerContext) {
    if (hasSingleLocallyControlledKingdom(localPlayerContext)
        && localPlayerContext.isLocallyControlled(notification.kingdom)) {
        return "You";
    }

    return (notification.kingdom == KingdomId::White) ? "White" : "Black";
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

inline std::string gameplayNotificationMessage(const GameplayNotification& notification,
                                               const LocalPlayerContext& localPlayerContext) {
    const std::string recipientLabel = gameplayNotificationRecipientLabel(notification, localPlayerContext);
    switch (notification.kind) {
        case GameplayNotificationKind::ChestReward:
            switch (notification.chestReward.type) {
                case ChestRewardType::Gold:
                    return recipientLabel + " gained " + describeChestReward(notification.chestReward) + ".";
                case ChestRewardType::MovementPointsMaxBonus:
                case ChestRewardType::BuildPointsMaxBonus:
                    return recipientLabel + " permanently gained " + describeChestReward(notification.chestReward) + ".";
            }
            break;
    }

    return "A gameplay event occurred.";
}