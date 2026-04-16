#include "Multiplayer/MultiplayerRuntime.hpp"

#include <SFML/System/Clock.hpp>

#include "Multiplayer/PasswordUtils.hpp"
#include "Multiplayer/Protocol.hpp"
#include "Save/SaveData.hpp"
#include "Save/SaveManager.hpp"

namespace {

void writeMultiplayerError(std::string* errorMessage, const std::string& message) {
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool waitForServerInfo(MultiplayerClient& client,
                       MultiplayerServerInfo& serverInfo,
                       sf::Time timeout,
                       std::string* errorMessage) {
    sf::Clock clock;
    while (clock.getElapsedTime() < timeout) {
        client.update();
        while (client.hasPendingEvent()) {
            const auto event = client.popNextEvent();
            if (event.type == MultiplayerClient::Event::Type::ServerInfoReceived) {
                serverInfo = event.serverInfo;
                return true;
            }

            if (event.type == MultiplayerClient::Event::Type::Disconnected
                || event.type == MultiplayerClient::Event::Type::Error) {
                writeMultiplayerError(
                    errorMessage,
                    event.message.empty() ? "The multiplayer host did not respond." : event.message);
                client.disconnect();
                return false;
            }
        }
    }

    writeMultiplayerError(errorMessage, "The multiplayer host did not answer the ping request.");
    client.disconnect();
    return false;
}

bool waitForJoinSnapshot(MultiplayerClient& client,
                         SaveManager& saveManager,
                         SaveData& snapshotData,
                         sf::Time timeout,
                         std::string* errorMessage) {
    bool accepted = false;
    bool receivedSnapshot = false;
    sf::Clock clock;
    while (clock.getElapsedTime() < timeout && !receivedSnapshot) {
        client.update();
        while (client.hasPendingEvent()) {
            const auto event = client.popNextEvent();
            if (event.type == MultiplayerClient::Event::Type::JoinRejected) {
                writeMultiplayerError(
                    errorMessage,
                    event.message.empty() ? "The multiplayer host rejected the join request." : event.message);
                client.disconnect();
                return false;
            }

            if (event.type == MultiplayerClient::Event::Type::JoinAccepted) {
                accepted = true;
                continue;
            }

            if (event.type == MultiplayerClient::Event::Type::SnapshotReceived) {
                if (!saveManager.deserialize(event.serializedSaveData, snapshotData)) {
                    writeMultiplayerError(errorMessage, "The multiplayer host sent an invalid game snapshot.");
                    client.disconnect();
                    return false;
                }

                receivedSnapshot = true;
                break;
            }

            if (event.type == MultiplayerClient::Event::Type::Disconnected
                || event.type == MultiplayerClient::Event::Type::Error) {
                writeMultiplayerError(
                    errorMessage,
                    event.message.empty() ? "Lost connection to the multiplayer host." : event.message);
                client.disconnect();
                return false;
            }
        }
    }

    if (!accepted || !receivedSnapshot) {
        writeMultiplayerError(errorMessage, "The multiplayer host did not complete the join handshake in time.");
        client.disconnect();
        return false;
    }

    return true;
}

} // namespace

void MultiplayerRuntime::resetConnections(const std::string& hostDisconnectReason) {
    if (m_server.isRunning()) {
        m_server.sendDisconnectNotice(hostDisconnectReason, nullptr);
    }

    m_server.stop();
    m_client.disconnect();
    m_lanHostRemoteSessionEstablished = false;
    m_hostJoinHint.clear();
}

void MultiplayerRuntime::clearReconnectState() {
    m_reconnectState = {};
}

void MultiplayerRuntime::cacheReconnectRequest(const MultiplayerJoinCredentials& request) {
    m_reconnectState.available = true;
    m_reconnectState.awaitingReconnect = false;
    m_reconnectState.reconnectAttemptInProgress = false;
    m_reconnectState.lastErrorMessage.clear();
    m_reconnectState.request = request;
}

bool MultiplayerRuntime::startHostIfNeeded(const GameSessionConfig& session,
                                           const std::string& saveName,
                                           std::string* errorMessage) {
    m_lanHostRemoteSessionEstablished = false;
    m_hostJoinHint.clear();

    if (!session.multiplayer.enabled) {
        return true;
    }

    if (!m_server.start(static_cast<unsigned short>(session.multiplayer.port),
                        saveName,
                        session.multiplayer,
                        errorMessage)) {
        return false;
    }

    const sf::IpAddress localAddress = sf::IpAddress::getLocalAddress();
    if (localAddress == sf::IpAddress::None) {
        m_hostJoinHint = "LAN port " + std::to_string(session.multiplayer.port);
    } else {
        m_hostJoinHint = localAddress.toString() + ":" + std::to_string(session.multiplayer.port);
    }

    return true;
}

bool MultiplayerRuntime::pushSnapshotIfConnected(bool lanHost,
                                                 const std::string& serializedSaveData,
                                                 std::string* errorMessage) {
    if (!lanHost || !m_server.hasAuthenticatedClient()) {
        return true;
    }

    return m_server.sendSnapshot(serializedSaveData, errorMessage);
}

bool MultiplayerRuntime::submitTurnSubmission(const std::vector<TurnCommand>& commands,
                                              std::string* errorMessage) {
    if (!m_client.isAuthenticated()) {
        writeMultiplayerError(errorMessage, "The multiplayer host connection is not authenticated.");
        return false;
    }

    return m_client.sendTurnSubmission(commands, errorMessage);
}

bool MultiplayerRuntime::sendTurnRejected(const std::string& reason,
                                          std::string* errorMessage) {
    return m_server.sendTurnRejected(reason, errorMessage);
}

bool MultiplayerRuntime::join(const MultiplayerJoinCredentials& request,
                              SaveManager& saveManager,
                              SaveData& snapshotData,
                              std::string* errorMessage) {
    const sf::IpAddress address(request.host);
    if (address == sf::IpAddress::None) {
        writeMultiplayerError(errorMessage, "Invalid server IP address.");
        return false;
    }

    if (!m_client.connect(address,
                          static_cast<unsigned short>(request.port),
                          sf::seconds(3.f),
                          errorMessage)) {
        return false;
    }
    if (!m_client.requestServerInfo(errorMessage)) {
        m_client.disconnect();
        return false;
    }

    MultiplayerServerInfo serverInfo;
    if (!waitForServerInfo(m_client, serverInfo, sf::seconds(3.f), errorMessage)) {
        return false;
    }
    if (!serverInfo.multiplayerEnabled) {
        writeMultiplayerError(errorMessage, "This server is not hosting a multiplayer save.");
        m_client.disconnect();
        return false;
    }
    if (serverInfo.protocolVersion != kCurrentMultiplayerProtocolVersion) {
        writeMultiplayerError(errorMessage, "The multiplayer host uses an incompatible protocol version.");
        m_client.disconnect();
        return false;
    }
    if (!serverInfo.joinable) {
        writeMultiplayerError(errorMessage, "The multiplayer server is already occupied.");
        m_client.disconnect();
        return false;
    }

    const std::string digest = MultiplayerPasswordUtils::computePasswordDigest(
        request.password,
        serverInfo.passwordSalt);
    if (!m_client.sendJoinRequest(digest, errorMessage)) {
        m_client.disconnect();
        return false;
    }

    if (!waitForJoinSnapshot(m_client, saveManager, snapshotData, sf::seconds(5.f), errorMessage)) {
        return false;
    }

    cacheReconnectRequest(request);
    m_reconnectState.reconnectAttemptInProgress = false;
    return true;
}

void MultiplayerRuntime::update() {
    if (m_server.isRunning()) {
        m_server.update();
    }

    if (m_client.isConnected()) {
        m_client.update();
    }
}

void MultiplayerRuntime::noteReconnectAwaiting(const std::string& message) {
    m_reconnectState.awaitingReconnect = true;
    m_reconnectState.reconnectAttemptInProgress = false;
    m_reconnectState.lastErrorMessage = message;
}

void MultiplayerRuntime::noteReconnectRecovered() {
    m_reconnectState.awaitingReconnect = false;
    m_reconnectState.reconnectAttemptInProgress = false;
    m_reconnectState.lastErrorMessage.clear();
}