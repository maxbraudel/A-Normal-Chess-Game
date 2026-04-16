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
    packet << snapshot.serializedSaveData;
    return true;
}

bool readPacket(sf::Packet& packet, MultiplayerStateSnapshot& snapshot) {
    return static_cast<bool>(packet >> snapshot.serializedSaveData);
}

bool writePacket(sf::Packet& packet, const MultiplayerTurnSubmission& submission) {
    packet << static_cast<sf::Uint32>(submission.commands.size());
    for (const auto& command : submission.commands) {
        if (!writeTurnCommand(packet, command)) {
            return false;
        }
    }

    return true;
}

bool readPacket(sf::Packet& packet, MultiplayerTurnSubmission& submission) {
    sf::Uint32 commandCount = 0;
    if (!(packet >> commandCount)) {
        return false;
    }

    submission.commands.clear();
    submission.commands.reserve(commandCount);
    for (sf::Uint32 index = 0; index < commandCount; ++index) {
        TurnCommand command;
        if (!readTurnCommand(packet, command)) {
            return false;
        }
        submission.commands.push_back(command);
    }

    return true;
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