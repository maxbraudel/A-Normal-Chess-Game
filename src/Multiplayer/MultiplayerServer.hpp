#pragma once

#include <SFML/Network.hpp>

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "Core/GameSessionConfig.hpp"
#include "Multiplayer/Protocol.hpp"

class MultiplayerServer {
public:
    struct Event {
        enum class Type {
            ClientConnected,
            ClientDisconnected,
            ClientConnectionInterrupted,
            TurnSubmitted,
            Error
        };

        Type type = Type::Error;
        std::string message;
        std::vector<TurnCommand> commands;
    };

    bool start(unsigned short port,
               const std::string& saveName,
               const MultiplayerConfig& config,
               std::string* errorMessage = nullptr);
    void stop();
    void update();

    bool isRunning() const { return m_running; }
    bool hasAuthenticatedClient() const { return m_clientAuthenticated; }
    bool hasPendingEvent() const { return !m_events.empty(); }
    Event popNextEvent();

    bool sendSnapshot(const std::string& serializedSaveData, std::string* errorMessage = nullptr);
    bool sendTurnRejected(const std::string& reason, std::string* errorMessage = nullptr);
    bool sendDisconnectNotice(const std::string& reason, std::string* errorMessage = nullptr);

private:
    void resetClientState();
    void handleClientTransportLoss(const std::string& authenticatedMessage,
                                   const std::string& interruptedMessage);
    void pushEvent(Event::Type type, const std::string& message, const std::vector<TurnCommand>& commands = {});
    bool sendPacket(sf::Packet& packet, std::string* errorMessage = nullptr);
    void handlePacket(sf::Packet& packet);
    bool sendServerInfo();
    bool sendJoinResponse(bool accepted, const std::string& reason);

    sf::TcpListener m_listener;
    std::unique_ptr<sf::TcpSocket> m_clientSocket;
    bool m_running = false;
    bool m_clientAuthenticated = false;
    unsigned short m_port = 0;
    std::string m_saveName;
    MultiplayerConfig m_config;
    std::deque<Event> m_events;
};