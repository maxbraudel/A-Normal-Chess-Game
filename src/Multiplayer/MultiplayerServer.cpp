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
    if (m_clientSocket) {
        m_clientSocket->disconnect();
        m_clientSocket.reset();
    }

    m_listener.close();
    m_running = false;
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
    if (m_clientSocket) {
        m_clientSocket->disconnect();
        m_clientSocket.reset();
    }

    m_clientAuthenticated = false;
}

void MultiplayerServer::handleClientTransportLoss(const std::string& authenticatedMessage,
                                                 const std::string& interruptedMessage) {
    const bool hadAuthenticatedClient = m_clientAuthenticated;
    resetClientState();
    pushEvent(hadAuthenticatedClient ? Event::Type::ClientDisconnected
                                     : Event::Type::ClientConnectionInterrupted,
              hadAuthenticatedClient ? authenticatedMessage : interruptedMessage);
}

void MultiplayerServer::pushEvent(Event::Type type,
                                  const std::string& message,
                                  const std::vector<TurnCommand>& commands,
                                  const MultiplayerTurnPreview& turnPreview) {
    m_events.push_back(Event{type, message, commands, turnPreview});
}

bool MultiplayerServer::sendPacket(sf::Packet& packet, std::string* errorMessage) {
    if (!m_clientSocket) {
        writeError(errorMessage, "No multiplayer client is currently connected.");
        return false;
    }

    m_clientSocket->setBlocking(true);
    const sf::Socket::Status status = m_clientSocket->send(packet);
    m_clientSocket->setBlocking(false);
    if (status != sf::Socket::Done) {
        writeError(errorMessage, "Failed to send multiplayer packet to the client.");
        handleClientTransportLoss("Multiplayer client disconnected.",
                                  "A multiplayer connection attempt was interrupted.");
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
    return sendSnapshot(serializedSaveData, {}, errorMessage);
}

bool MultiplayerServer::sendSnapshot(const std::string& serializedSaveData,
                                     const std::vector<GameplayNotification>& notifications,
                                     std::string* errorMessage) {
    if (!m_clientAuthenticated) {
        writeError(errorMessage, "Cannot send a multiplayer snapshot without an authenticated client.");
        return false;
    }

    sf::Packet packet = createPacket(MultiplayerMessageType::StateSnapshot);
    writePacket(packet, MultiplayerStateSnapshot{serializedSaveData, notifications});
    return sendPacket(packet, errorMessage);
}

bool MultiplayerServer::sendTurnPreview(const MultiplayerTurnPreview& preview, std::string* errorMessage) {
    if (!m_clientAuthenticated) {
        writeError(errorMessage, "Cannot send a turn preview without an authenticated client.");
        return false;
    }

    sf::Packet packet = createPacket(MultiplayerMessageType::TurnPreview);
    writePacket(packet, preview);
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
    if (!m_clientSocket) {
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
            if (!sendJoinResponse(true, "")) {
                return;
            }
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

        case MultiplayerMessageType::TurnPreview: {
            if (!m_clientAuthenticated) {
                pushEvent(Event::Type::Error, "Rejected a turn preview from an unauthenticated client.");
                return;
            }

            MultiplayerTurnPreview preview;
            if (!readPacket(packet, preview)) {
                pushEvent(Event::Type::Error, "Received an invalid turn preview packet.");
                return;
            }

            pushEvent(Event::Type::TurnPreviewReceived, "Remote turn preview updated.", {}, preview);
            break;
        }

        case MultiplayerMessageType::DisconnectNotice:
            handleClientTransportLoss("Remote player disconnected.",
                                      "A multiplayer connection attempt was interrupted.");
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

    if (!m_clientSocket) {
        auto nextSocket = std::make_unique<sf::TcpSocket>();
        const sf::Socket::Status acceptStatus = m_listener.accept(*nextSocket);
        if (acceptStatus == sf::Socket::Done) {
            nextSocket->setBlocking(false);
            m_clientSocket = std::move(nextSocket);
        }
    }

    if (!m_clientSocket) {
        return;
    }

    while (true) {
        sf::Packet packet;
        const sf::Socket::Status status = m_clientSocket->receive(packet);
        if (status == sf::Socket::Done) {
            handlePacket(packet);
            if (!m_clientSocket) {
                break;
            }
            continue;
        }

        if (status == sf::Socket::Disconnected) {
            handleClientTransportLoss("Multiplayer client disconnected.",
                                      "A multiplayer connection attempt was interrupted before joining.");
        }

        break;
    }
}