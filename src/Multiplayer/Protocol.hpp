#pragma once

#include <SFML/Network.hpp>

#include <string>
#include <vector>

#include "Core/GameplayNotification.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Systems/TurnCommand.hpp"

enum class MultiplayerMessageType : sf::Uint8 {
    ServerInfoRequest = 1,
    ServerInfoResponse,
    JoinRequest,
    JoinResponse,
    StateSnapshot,
    TurnSubmission,
    TurnPreview,
    TurnRejected,
    DisconnectNotice
};

struct MultiplayerServerInfo {
    sf::Uint32 protocolVersion = kCurrentMultiplayerProtocolVersion;
    std::string saveName;
    std::string passwordSalt;
    bool multiplayerEnabled = false;
    bool joinable = false;
};

struct MultiplayerJoinRequest {
    std::string passwordDigest;
};

struct MultiplayerJoinResponse {
    bool accepted = false;
    std::string reason;
};

struct MultiplayerStateSnapshot {
    std::string serializedSaveData;
    std::vector<GameplayNotification> notifications;
};

struct MultiplayerTurnSubmission {
    std::vector<TurnCommand> commands;
};

struct MultiplayerTurnPreview {
    int turnNumber = 1;
    KingdomId activeKingdom = KingdomId::White;
    std::uint64_t pendingStateRevision = 0;
    std::vector<TurnCommand> commands;
};

struct MultiplayerTurnRejected {
    std::string reason;
};

struct MultiplayerDisconnectNotice {
    std::string reason;
};

sf::Packet createPacket(MultiplayerMessageType type);
bool extractMessageType(sf::Packet& packet, MultiplayerMessageType& type);

bool writePacket(sf::Packet& packet, const MultiplayerServerInfo& info);
bool readPacket(sf::Packet& packet, MultiplayerServerInfo& info);

bool writePacket(sf::Packet& packet, const MultiplayerJoinRequest& request);
bool readPacket(sf::Packet& packet, MultiplayerJoinRequest& request);

bool writePacket(sf::Packet& packet, const MultiplayerJoinResponse& response);
bool readPacket(sf::Packet& packet, MultiplayerJoinResponse& response);

bool writePacket(sf::Packet& packet, const MultiplayerStateSnapshot& snapshot);
bool readPacket(sf::Packet& packet, MultiplayerStateSnapshot& snapshot);

bool writePacket(sf::Packet& packet, const MultiplayerTurnSubmission& submission);
bool readPacket(sf::Packet& packet, MultiplayerTurnSubmission& submission);

bool writePacket(sf::Packet& packet, const MultiplayerTurnPreview& preview);
bool readPacket(sf::Packet& packet, MultiplayerTurnPreview& preview);

bool writePacket(sf::Packet& packet, const MultiplayerTurnRejected& rejection);
bool readPacket(sf::Packet& packet, MultiplayerTurnRejected& rejection);

bool writePacket(sf::Packet& packet, const MultiplayerDisconnectNotice& notice);
bool readPacket(sf::Packet& packet, MultiplayerDisconnectNotice& notice);