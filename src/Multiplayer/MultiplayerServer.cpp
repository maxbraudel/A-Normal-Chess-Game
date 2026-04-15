#include "Multiplayer/MultiplayerServer.hpp"

namespace {

void writeError(std::string* errorMessage, const std::string& message) {
    if (errorMessage) {
        *errorMessage = message;
    }
}

} // namespace

bool MultiplayerServer::start(unsigned short port,
                              const std::string& saveName,
                              const MultiplayerConfig& config,
                              std::string* errorMessage) {
    stop();

    if (m_listener.listen(port) != sf::Socket::Done) {
        writeError(errorMessage, "Unable to listen on the requested multiplayer port.");
        return false;
    }

    m_listener.setBlocking(false);
    m_running = true;
    m_port = port;
    m_saveName = saveName;
    m_config = config;
    m_events.clear();
    return true;
}

void MultiplayerServer::stop() {
    if (m_hasClientSocket) {
        m_clientSocket.disconnect();
    }

    m_listener.close();
    m_running = false;
    m_hasClientSocket = false;
    m_clientAuthenticated = false;
    m_events.clear();
}

MultiplayerServer::Event MultiplayerServer::popNextEvent() {
    Event event;
    if (!m_events.empty()) {
        event = m_events.front();
        m_events.pop_front();
    }

    return event;
}

void MultiplayerServer::resetClientState() {
    if (m_hasClientSocket) {
        m_clientSocket.disconnect();
    }

    m_hasClientSocket = false;
    m_clientAuthenticated = false;
}

void MultiplayerServer::pushEvent(Event::Type type,
                                  const std::string& message,
                                  const std::vector<TurnCommand>& commands) {
    m_events.push_back(Event{type, message, commands});
}

bool MultiplayerServer::sendPacket(sf::Packet& packet, std::string* errorMessage) {
    if (!m_hasClientSocket) {
        writeError(errorMessage, "No multiplayer client is currently connected.");
        return false;
    }

    const sf::Socket::Status status = m_clientSocket.send(packet);
    if (status != sf::Socket::Done) {
        writeError(errorMessage, "Failed to send multiplayer packet to the client.");
        return false;
    }

    return true;
}

bool MultiplayerServer::sendServerInfo() {
    sf::Packet packet = createPacket(MultiplayerMessageType::ServerInfoResponse);
    MultiplayerServerInfo info;
    info.protocolVersion = m_config.protocolVersion;
    info.saveName = m_saveName;
    info.passwordSalt = m_config.passwordSalt;
    info.multiplayerEnabled = m_config.enabled;
    info.joinable = !m_clientAuthenticated;
    writePacket(packet, info);
    return sendPacket(packet, nullptr);
}

bool MultiplayerServer::sendJoinResponse(bool accepted, const std::string& reason) {
    sf::Packet packet = createPacket(MultiplayerMessageType::JoinResponse);
    writePacket(packet, MultiplayerJoinResponse{accepted, reason});
    return sendPacket(packet, nullptr);
}

bool MultiplayerServer::sendSnapshot(const std::string& serializedSaveData, std::string* errorMessage) {
    if (!m_clientAuthenticated) {
        writeError(errorMessage, "Cannot send a multiplayer snapshot without an authenticated client.");
        return false;
    }

    sf::Packet packet = createPacket(MultiplayerMessageType::StateSnapshot);
    writePacket(packet, MultiplayerStateSnapshot{serializedSaveData});
    return sendPacket(packet, errorMessage);
}

bool MultiplayerServer::sendTurnRejected(const std::string& reason, std::string* errorMessage) {
    if (!m_clientAuthenticated) {
        writeError(errorMessage, "Cannot send a turn rejection without an authenticated client.");
        return false;
    }

    sf::Packet packet = createPacket(MultiplayerMessageType::TurnRejected);
    writePacket(packet, MultiplayerTurnRejected{reason});
    return sendPacket(packet, errorMessage);
}

bool MultiplayerServer::sendDisconnectNotice(const std::string& reason, std::string* errorMessage) {
    if (!m_hasClientSocket) {
        return true;
    }

    sf::Packet packet = createPacket(MultiplayerMessageType::DisconnectNotice);
    writePacket(packet, MultiplayerDisconnectNotice{reason});
    return sendPacket(packet, errorMessage);
}

void MultiplayerServer::handlePacket(sf::Packet& packet) {
    MultiplayerMessageType type;
    if (!extractMessageType(packet, type)) {
        pushEvent(Event::Type::Error, "Received an invalid multiplayer packet.");
        return;
    }

    switch (type) {
        case MultiplayerMessageType::ServerInfoRequest:
            sendServerInfo();
            break;

        case MultiplayerMessageType::JoinRequest: {
            MultiplayerJoinRequest request;
            if (!readPacket(packet, request)) {
                pushEvent(Event::Type::Error, "Received an invalid join request packet.");
                return;
            }

            if (m_clientAuthenticated) {
                sendJoinResponse(false, "A multiplayer client is already connected.");
                return;
            }

            if (request.passwordDigest != m_config.passwordHash) {
                sendJoinResponse(false, "Invalid multiplayer password.");
                return;
            }

            m_clientAuthenticated = true;
            sendJoinResponse(true, "");
            pushEvent(Event::Type::ClientConnected, "Multiplayer client connected.");
            break;
        }

        case MultiplayerMessageType::TurnSubmission: {
            if (!m_clientAuthenticated) {
                pushEvent(Event::Type::Error, "Rejected a turn submission from an unauthenticated client.");
                return;
            }

            MultiplayerTurnSubmission submission;
            if (!readPacket(packet, submission)) {
                pushEvent(Event::Type::Error, "Received an invalid turn submission packet.");
                return;
            }

            pushEvent(Event::Type::TurnSubmitted, "Remote turn submitted.", submission.commands);
            break;
        }

        case MultiplayerMessageType::DisconnectNotice:
            resetClientState();
            pushEvent(Event::Type::ClientDisconnected, "Remote player disconnected.");
            break;

        default:
            pushEvent(Event::Type::Error, "Received an unexpected multiplayer packet type on the server.");
            break;
    }
}

void MultiplayerServer::update() {
    if (!m_running) {
        return;
    }

    if (!m_hasClientSocket) {
        const sf::Socket::Status acceptStatus = m_listener.accept(m_clientSocket);
        if (acceptStatus == sf::Socket::Done) {
            m_clientSocket.setBlocking(false);
            m_hasClientSocket = true;
        }
    }

    if (!m_hasClientSocket) {
        return;
    }

    while (true) {
        sf::Packet packet;
        const sf::Socket::Status status = m_clientSocket.receive(packet);
        if (status == sf::Socket::Done) {
            handlePacket(packet);
            continue;
        }

        if (status == sf::Socket::Disconnected) {
            resetClientState();
            pushEvent(Event::Type::ClientDisconnected, "Multiplayer client disconnected.");
        }

        break;
    }
}