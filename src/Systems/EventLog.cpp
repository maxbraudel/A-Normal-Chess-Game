#include "Systems/EventLog.hpp"

namespace {

const char* pieceTypeDisplayName(PieceType type) {
    switch (type) {
        case PieceType::Pawn:
            return "Pawn";
        case PieceType::Knight:
            return "Knight";
        case PieceType::Bishop:
            return "Bishop";
        case PieceType::Rook:
            return "Rook";
        case PieceType::Queen:
            return "Queen";
        case PieceType::King:
            return "King";
    }

    return "Piece";
}

std::string moveFallbackMessage(PieceType pieceType) {
    return "Moved " + std::string(pieceTypeDisplayName(pieceType)) + ".";
}

} // namespace

void EventLog::log(int turn, KingdomId kingdom, const std::string& msg) {
    Event event;
    event.turnNumber = turn;
    event.kingdom = kingdom;
    event.message = msg;
    m_events.push_back(event);
}

void EventLog::log(const Event& event) {
    m_events.push_back(event);
}

void EventLog::logMove(int turn,
                       KingdomId kingdom,
                       PieceType pieceType,
                       sf::Vector2i destinationCell,
                       const std::array<bool, kNumKingdoms>& destinationHiddenByKingdom) {
    Event event;
    event.turnNumber = turn;
    event.kingdom = kingdom;
    event.message = moveFallbackMessage(pieceType);
    event.kind = Event::Kind::Move;
    event.pieceType = pieceType;
    event.destinationCell = destinationCell;
    event.destinationHiddenByKingdom = destinationHiddenByKingdom;
    m_events.push_back(event);
}

const std::vector<EventLog::Event>& EventLog::getEvents() const {
    return m_events;
}

std::vector<EventLog::Event> EventLog::getEventsForKingdom(KingdomId id) const {
    std::vector<Event> result;
    for (const auto& e : m_events)
        if (e.kingdom == id) result.push_back(e);
    return result;
}

void EventLog::clear() { m_events.clear(); }
