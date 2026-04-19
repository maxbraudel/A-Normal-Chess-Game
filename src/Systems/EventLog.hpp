#pragma once
#include <array>
#include <string>
#include <vector>

#include <SFML/System/Vector2.hpp>

#include "Kingdom/KingdomId.hpp"
#include "Units/PieceType.hpp"

class EventLog {
public:
    struct Event {
        enum class Kind {
            Message = 0,
            Move
        };

        int turnNumber;
        KingdomId kingdom;
        std::string message;
        Kind kind = Kind::Message;
        PieceType pieceType = PieceType::Pawn;
        sf::Vector2i destinationCell{0, 0};
        std::array<bool, kNumKingdoms> destinationHiddenByKingdom{};

        bool isDestinationHiddenFor(KingdomId observer) const {
            return destinationHiddenByKingdom[static_cast<std::size_t>(kingdomIndex(observer))];
        }
    };

    void log(int turn, KingdomId kingdom, const std::string& msg);
    void log(const Event& event);
    void logMove(int turn,
                 KingdomId kingdom,
                 PieceType pieceType,
                 sf::Vector2i destinationCell,
                 const std::array<bool, kNumKingdoms>& destinationHiddenByKingdom);
    const std::vector<Event>& getEvents() const;
    std::vector<Event> getEventsForKingdom(KingdomId id) const;
    void clear();

private:
    std::vector<Event> m_events;
};
