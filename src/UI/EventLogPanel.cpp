#include "UI/EventLogPanel.hpp"
#include "Systems/EventLog.hpp"
#include "Kingdom/KingdomId.hpp"

void EventLogPanel::init(tgui::Gui& gui) {
    m_panel = tgui::Panel::create({200, 400});
    m_panel->setPosition({"&.width - 210", "50"});
    m_panel->getRenderer()->setBackgroundColor(tgui::Color(50, 50, 50, 220));
    m_panel->getRenderer()->setBorderColor(tgui::Color::White);
    m_panel->getRenderer()->setBorders({1});
    gui.add(m_panel, "EventLogPanel");

    auto titleLabel = tgui::Label::create("Event Log");
    titleLabel->setPosition({10, 5});
    titleLabel->setTextSize(16);
    titleLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(titleLabel);

    m_listView = tgui::ListView::create();
    m_listView->setPosition({5, 30});
    m_listView->setSize({190, 360});
    m_listView->addColumn("Turn", 40);
    m_listView->addColumn("Event", 140);
    m_listView->getRenderer()->setBackgroundColor(tgui::Color(30, 30, 30));
    m_listView->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_listView);

    m_panel->setVisible(false);
}

void EventLogPanel::show(const EventLog& log) {
    if (!m_panel) return;

    m_listView->removeAllItems();
    const auto& events = log.getEvents();
    // Show most recent first, max 50
    int start = static_cast<int>(events.size()) - 1;
    int count = 0;
    for (int i = start; i >= 0 && count < 50; --i, ++count) {
        const auto& ev = events[static_cast<std::size_t>(i)];
        std::string kingdom = (ev.kingdom == KingdomId::White) ? "W" : "B";
        m_listView->addItem({std::to_string(ev.turnNumber) + " " + kingdom, ev.message});
    }

    m_panel->setVisible(true);
}

void EventLogPanel::hide() { if (m_panel) m_panel->setVisible(false); }
