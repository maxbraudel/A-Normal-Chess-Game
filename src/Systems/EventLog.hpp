#pragma once
#include <vector>
#include <string>
#include "Kingdom/KingdomId.hpp"

class EventLog {
public:
    struct Event {
        int turnNumber;
        KingdomId kingdom;
        std::string message;
    };

    void log(int turn, KingdomId kingdom, const std::string& msg);
    const std::vector<Event>& getEvents() const;
    std::vector<Event> getEventsForKingdom(KingdomId id) const;
    void clear();

private:
    std::vector<Event> m_events;
};
