#include "UI/EventLogPanel.hpp"

#include "UI/HUDLayout.hpp"

void EventLogPanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    auto titleLabel = tgui::Label::create("Historique");
    titleLabel->setPosition({10, 5});
    HUDLayout::styleSidebarTitle(titleLabel);
    m_panel->add(titleLabel);

    m_listView = tgui::ListView::create();
    m_listView->setPosition({8, 34});
    m_listView->setSize({"&.width - 16", "&.height - 42"});
    m_listView->addColumn("Turn", 48);
    m_listView->addColumn("Kingdom", 126);
    m_listView->addColumn("Action", 144);
    m_listView->getRenderer()->setBackgroundColor(tgui::Color(24, 24, 24, 210));
    m_listView->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_listView);

    m_panel->setVisible(false);
}

void EventLogPanel::show() {
    if (m_panel) {
        m_panel->setVisible(true);
    }
}

void EventLogPanel::update(const std::vector<InGameEventRow>& rows) {
    if (!m_panel || !m_listView) {
        return;
    }

    m_listView->removeAllItems();
    for (const auto& row : rows) {
        m_listView->addItem({std::to_string(row.turnNumber), row.actorLabel, row.actionLabel});
    }
}

void EventLogPanel::hide() { if (m_panel) m_panel->setVisible(false); }
