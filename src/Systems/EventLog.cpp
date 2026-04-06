#include "Systems/EventLog.hpp"

void EventLog::log(int turn, KingdomId kingdom, const std::string& msg) {
    m_events.push_back({turn, kingdom, msg});
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
