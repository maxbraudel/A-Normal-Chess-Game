#include "Multiplayer/MultiplayerClient.hpp"

namespace {

void writeError(std::string* errorMessage, const std::string& message) {
    if (errorMessage) {
        *errorMessage = message;
    }
}

} // namespace

bool MultiplayerClient::connect(const sf::IpAddress& address,
                                unsigned short port,
                                sf::Time timeout,
                                std::string* errorMessage) {
    disconnect();

    const sf::Socket::Status status = m_socket.connect(address, port, timeout);
    if (status != sf::Socket::Done) {
        writeError(errorMessage, "Unable to connect to the multiplayer host.");
        return false;
    }

    m_socket.setBlocking(false);
    m_connected = true;
    return true;
}

void MultiplayerClient::disconnect() {
    if (m_connected) {
        m_socket.disconnect();
    }

    m_connected = false;
    m_authenticated = false;
    m_events.clear();
}

MultiplayerClient::Event MultiplayerClient::popNextEvent() {
    Event event;
    if (!m_events.empty()) {
        event = m_events.front();
        m_events.pop_front();
    }

    return event;
}

void MultiplayerClient::pushEvent(const Event& event) {
    m_events.push_back(event);
}

bool MultiplayerClient::sendPacket(sf::Packet& packet, std::string* errorMessage) {
    if (!m_connected) {
        writeError(errorMessage, "No multiplayer host connection is active.");
        return false;
    }

    const sf::Socket::Status status = m_socket.send(packet);
    if (status != sf::Socket::Done) {
        writeError(errorMessage, "Failed to send multiplayer data to the host.");
        return false;
    }

    return true;
}

bool MultiplayerClient::requestServerInfo(std::string* errorMessage) {
    sf::Packet packet = createPacket(MultiplayerMessageType::ServerInfoRequest);
    return sendPacket(packet, errorMessage);
}

bool MultiplayerClient::sendJoinRequest(const std::string& passwordDigest, std::string* errorMessage) {
    sf::Packet packet = createPacket(MultiplayerMessageType::JoinRequest);
    writePacket(packet, MultiplayerJoinRequest{passwordDigest});
    return sendPacket(packet, errorMessage);
}

bool MultiplayerClient::sendTurnSubmission(const std::vector<TurnCommand>& commands, std::string* errorMessage) {
    sf::Packet packet = createPacket(MultiplayerMessageType::TurnSubmission);
    writePacket(packet, MultiplayerTurnSubmission{commands});
    return sendPacket(packet, errorMessage);
}

void MultiplayerClient::handlePacket(sf::Packet& packet) {
    MultiplayerMessageType type;
    if (!extractMessageType(packet, type)) {
        pushEvent(Event{Event::Type::Error, {}, "Received an invalid multiplayer packet from the host.", {}});
        return;
    }

    switch (type) {
        case MultiplayerMessageType::ServerInfoResponse: {
            MultiplayerServerInfo info;
            if (!readPacket(packet, info)) {
                pushEvent(Event{Event::Type::Error, {}, "Received an invalid server info packet.", {}});
                return;
            }
            pushEvent(Event{Event::Type::ServerInfoReceived, info, "", {}});
            break;
        }

        case MultiplayerMessageType::JoinResponse: {
            MultiplayerJoinResponse response;
            if (!readPacket(packet, response)) {
                pushEvent(Event{Event::Type::Error, {}, "Received an invalid join response packet.", {}});
                return;
            }

            m_authenticated = response.accepted;
            pushEvent(Event{response.accepted ? Event::Type::JoinAccepted : Event::Type::JoinRejected,
                            {}, response.reason, {}});
            break;
        }

        case MultiplayerMessageType::StateSnapshot: {
            MultiplayerStateSnapshot snapshot;
            if (!readPacket(packet, snapshot)) {
                pushEvent(Event{Event::Type::Error, {}, "Received an invalid state snapshot packet.", {}});
                return;
            }

            pushEvent(Event{Event::Type::SnapshotReceived, {}, "", snapshot.serializedSaveData});
            break;
        }

        case MultiplayerMessageType::TurnRejected: {
            MultiplayerTurnRejected rejection;
            if (!readPacket(packet, rejection)) {
                pushEvent(Event{Event::Type::Error, {}, "Received an invalid turn rejection packet.", {}});
                return;
            }

            pushEvent(Event{Event::Type::TurnRejected, {}, rejection.reason, {}});
            break;
        }

        case MultiplayerMessageType::DisconnectNotice: {
            MultiplayerDisconnectNotice notice;
            if (!readPacket(packet, notice)) {
                notice.reason = "Disconnected from the multiplayer host.";
            }
            m_connected = false;
            m_authenticated = false;
            m_socket.disconnect();
            pushEvent(Event{Event::Type::Disconnected, {}, notice.reason, {}});
            break;
        }

        default:
            pushEvent(Event{Event::Type::Error, {}, "Received an unexpected multiplayer packet type on the client.", {}});
            break;
    }
}

void MultiplayerClient::update() {
    if (!m_connected) {
        return;
    }

    while (true) {
        sf::Packet packet;
        const sf::Socket::Status status = m_socket.receive(packet);
        if (status == sf::Socket::Done) {
            handlePacket(packet);
            continue;
        }

        if (status == sf::Socket::Disconnected) {
            m_connected = false;
            m_authenticated = false;
            m_socket.disconnect();
            pushEvent(Event{Event::Type::Disconnected, {}, "Disconnected from the multiplayer host.", {}});
        }

        break;
    }
}