#include "Multiplayer/Protocol.hpp"

namespace {

bool writeBool(sf::Packet& packet, bool value) {
    packet << static_cast<sf::Uint8>(value ? 1 : 0);
    return true;
}

bool readBool(sf::Packet& packet, bool& value) {
    sf::Uint8 encoded = 0;
    if (!(packet >> encoded)) {
        return false;
    }
    value = encoded != 0;
    return true;
}

bool writeTurnCommand(sf::Packet& packet, const TurnCommand& command);
bool readTurnCommand(sf::Packet& packet, TurnCommand& command);

bool writeTurnCommandVector(sf::Packet& packet, const std::vector<TurnCommand>& commands) {
    packet << static_cast<sf::Uint32>(commands.size());
    for (const TurnCommand& command : commands) {
        if (!writeTurnCommand(packet, command)) {
            return false;
        }
    }

    return true;
}

bool readTurnCommandVector(sf::Packet& packet, std::vector<TurnCommand>& commands) {
    sf::Uint32 commandCount = 0;
    if (!(packet >> commandCount)) {
        return false;
    }

    commands.clear();
    commands.reserve(commandCount);
    for (sf::Uint32 index = 0; index < commandCount; ++index) {
        TurnCommand command;
        if (!readTurnCommand(packet, command)) {
            return false;
        }
        commands.push_back(command);
    }

    return true;
}

bool writeTurnCommand(sf::Packet& packet, const TurnCommand& command) {
    packet << static_cast<sf::Uint8>(command.type)
           << command.pieceId
           << command.origin.x
           << command.origin.y
           << command.destination.x
           << command.destination.y
           << command.buildId
           << static_cast<sf::Int32>(command.buildingType)
           << command.buildOrigin.x
           << command.buildOrigin.y
           << static_cast<sf::Int32>(command.buildRotationQuarterTurns)
           << command.barracksId
           << static_cast<sf::Int32>(command.produceType)
           << command.upgradePieceId
           << static_cast<sf::Int32>(command.upgradeTarget)
           << command.formationId;
    return true;
}

bool readTurnCommand(sf::Packet& packet, TurnCommand& command) {
    sf::Uint8 type = 0;
    sf::Int32 buildingType = 0;
    sf::Int32 buildRotationQuarterTurns = 0;
    sf::Int32 produceType = 0;
    sf::Int32 upgradeTarget = 0;

    if (!(packet >> type
          >> command.pieceId
          >> command.origin.x
          >> command.origin.y
          >> command.destination.x
          >> command.destination.y
            >> command.buildId
          >> buildingType
          >> command.buildOrigin.x
          >> command.buildOrigin.y
          >> buildRotationQuarterTurns
          >> command.barracksId
          >> produceType
          >> command.upgradePieceId
          >> upgradeTarget
          >> command.formationId)) {
        return false;
    }

    command.type = static_cast<TurnCommand::Type>(type);
    command.buildingType = static_cast<BuildingType>(buildingType);
    command.buildRotationQuarterTurns = buildRotationQuarterTurns;
    command.produceType = static_cast<PieceType>(produceType);
    command.upgradeTarget = static_cast<PieceType>(upgradeTarget);
    return true;
}

bool writeChestReward(sf::Packet& packet, const ChestReward& reward) {
    packet << static_cast<sf::Uint8>(reward.type)
           << reward.amount;
    return true;
}

bool readChestReward(sf::Packet& packet, ChestReward& reward) {
    sf::Uint8 rewardType = 0;
    if (!(packet >> rewardType >> reward.amount)) {
        return false;
    }

    reward.type = static_cast<ChestRewardType>(rewardType);
    return true;
}

bool writeGameplayNotification(sf::Packet& packet, const GameplayNotification& notification) {
    packet << static_cast<sf::Uint8>(notification.kind)
           << static_cast<sf::Uint8>(notification.kingdom);

    switch (notification.kind) {
        case GameplayNotificationKind::ChestReward:
            return writeChestReward(packet, notification.chestReward);
    }

    return false;
}

bool readGameplayNotification(sf::Packet& packet, GameplayNotification& notification) {
    sf::Uint8 notificationKind = 0;
    sf::Uint8 kingdom = 0;
    if (!(packet >> notificationKind >> kingdom)) {
        return false;
    }

    notification.kind = static_cast<GameplayNotificationKind>(notificationKind);
    notification.kingdom = static_cast<KingdomId>(kingdom);

    switch (notification.kind) {
        case GameplayNotificationKind::ChestReward:
            return readChestReward(packet, notification.chestReward);
    }

    return false;
}

} // namespace

sf::Packet createPacket(MultiplayerMessageType type) {
    sf::Packet packet;
    packet << static_cast<sf::Uint8>(type);
    return packet;
}

bool extractMessageType(sf::Packet& packet, MultiplayerMessageType& type) {
    sf::Uint8 encodedType = 0;
    if (!(packet >> encodedType)) {
        return false;
    }

    type = static_cast<MultiplayerMessageType>(encodedType);
    return true;
}

bool writePacket(sf::Packet& packet, const MultiplayerServerInfo& info) {
    packet << info.protocolVersion
           << info.saveName
           << info.passwordSalt;
    writeBool(packet, info.multiplayerEnabled);
    writeBool(packet, info.joinable);
    return true;
}

bool readPacket(sf::Packet& packet, MultiplayerServerInfo& info) {
    if (!(packet >> info.protocolVersion >> info.saveName >> info.passwordSalt)) {
        return false;
    }

    return readBool(packet, info.multiplayerEnabled)
        && readBool(packet, info.joinable);
}

bool writePacket(sf::Packet& packet, const MultiplayerJoinRequest& request) {
    packet << request.passwordDigest;
    return true;
}

bool readPacket(sf::Packet& packet, MultiplayerJoinRequest& request) {
    return static_cast<bool>(packet >> request.passwordDigest);
}

bool writePacket(sf::Packet& packet, const MultiplayerJoinResponse& response) {
    writeBool(packet, response.accepted);
    packet << response.reason;
    return true;
}

bool readPacket(sf::Packet& packet, MultiplayerJoinResponse& response) {
    return readBool(packet, response.accepted)
        && static_cast<bool>(packet >> response.reason);
}

bool writePacket(sf::Packet& packet, const MultiplayerStateSnapshot& snapshot) {
    packet << snapshot.serializedSaveData
           << static_cast<sf::Uint32>(snapshot.notifications.size());
    for (const GameplayNotification& notification : snapshot.notifications) {
        if (!writeGameplayNotification(packet, notification)) {
            return false;
        }
    }
    return true;
}

bool readPacket(sf::Packet& packet, MultiplayerStateSnapshot& snapshot) {
    sf::Uint32 notificationCount = 0;
    if (!(packet >> snapshot.serializedSaveData >> notificationCount)) {
        return false;
    }

    snapshot.notifications.clear();
    snapshot.notifications.reserve(notificationCount);
    for (sf::Uint32 index = 0; index < notificationCount; ++index) {
        GameplayNotification notification;
        if (!readGameplayNotification(packet, notification)) {
            return false;
        }
        snapshot.notifications.push_back(notification);
    }

    return true;
}

bool writePacket(sf::Packet& packet, const MultiplayerTurnSubmission& submission) {
    return writeTurnCommandVector(packet, submission.commands);
}

bool readPacket(sf::Packet& packet, MultiplayerTurnSubmission& submission) {
    return readTurnCommandVector(packet, submission.commands);
}

bool writePacket(sf::Packet& packet, const MultiplayerTurnPreview& preview) {
    packet << preview.turnNumber
           << static_cast<sf::Uint8>(preview.activeKingdom)
           << static_cast<sf::Uint64>(preview.pendingStateRevision);
    return writeTurnCommandVector(packet, preview.commands);
}

bool readPacket(sf::Packet& packet, MultiplayerTurnPreview& preview) {
    sf::Uint8 activeKingdom = 0;
    sf::Uint64 pendingStateRevision = 0;
    if (!(packet >> preview.turnNumber >> activeKingdom >> pendingStateRevision)) {
        return false;
    }

    preview.activeKingdom = static_cast<KingdomId>(activeKingdom);
    preview.pendingStateRevision = static_cast<std::uint64_t>(pendingStateRevision);
    return readTurnCommandVector(packet, preview.commands);
}

bool writePacket(sf::Packet& packet, const MultiplayerTurnRejected& rejection) {
    packet << rejection.reason;
    return true;
}

bool readPacket(sf::Packet& packet, MultiplayerTurnRejected& rejection) {
    return static_cast<bool>(packet >> rejection.reason);
}

bool writePacket(sf::Packet& packet, const MultiplayerDisconnectNotice& notice) {
    packet << notice.reason;
    return true;
}

bool readPacket(sf::Packet& packet, MultiplayerDisconnectNotice& notice) {
    return static_cast<bool>(packet >> notice.reason);
}