#pragma once

#include <SFML/Network.hpp>

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "Core/GameplayNotification.hpp"
#include "Multiplayer/Protocol.hpp"

class MultiplayerClient {
public:
    struct Event {
        enum class Type {
            ServerInfoReceived,
            JoinAccepted,
            JoinRejected,
            SnapshotReceived,
            TurnRejected,
            Disconnected,
            Error
        };

        Type type = Type::Error;
        MultiplayerServerInfo serverInfo;
        std::string message;
        std::string serializedSaveData;
        std::vector<GameplayNotification> snapshotNotifications;
    };

    bool connect(const sf::IpAddress& address,
                 unsigned short port,
                 sf::Time timeout,
                 std::string* errorMessage = nullptr);
    void disconnect();
    void update();

    bool requestServerInfo(std::string* errorMessage = nullptr);
    bool sendJoinRequest(const std::string& passwordDigest, std::string* errorMessage = nullptr);
    bool sendTurnSubmission(const std::vector<TurnCommand>& commands, std::string* errorMessage = nullptr);

    bool isConnected() const { return m_connected; }
    bool isAuthenticated() const { return m_authenticated; }
    bool hasPendingEvent() const { return !m_events.empty(); }
    Event popNextEvent();

private:
    void handleTransportDisconnect(const std::string& message);
    void pushEvent(const Event& event);
    bool sendPacket(sf::Packet& packet, std::string* errorMessage = nullptr);
    void handlePacket(sf::Packet& packet);

    std::unique_ptr<sf::TcpSocket> m_socket;
    bool m_connected = false;
    bool m_authenticated = false;
    std::deque<Event> m_events;
};