#include "UI/EventLogPanel.hpp"

#include "UI/HUDLayout.hpp"

namespace {

constexpr std::size_t kTurnColumnIndex = 0;
constexpr std::size_t kKingdomColumnIndex = 1;
constexpr std::size_t kActionColumnIndex = 2;

void configureEventLogColumns(const tgui::ListView::Ptr& listView) {
    listView->setColumnWidth(kTurnColumnIndex, HUDLayout::kEventLogTurnColumnWidth);
    listView->setColumnWidth(kKingdomColumnIndex, HUDLayout::kEventLogKingdomColumnWidth);
    listView->setColumnWidth(kActionColumnIndex, 160.f);
    listView->setColumnExpanded(kActionColumnIndex, true);
}

} // namespace

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
    m_listView->addColumn("Turn", HUDLayout::kEventLogTurnColumnWidth);
    m_listView->addColumn("Kingdom", HUDLayout::kEventLogKingdomColumnWidth);
    m_listView->addColumn("Action", 160.f);
    configureEventLogColumns(m_listView);
    m_listView->getRenderer()->setBackgroundColor(tgui::Color(24, 24, 24, 210));
    m_listView->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_listView);

    m_listView->onSizeChange([this](const tgui::Vector2f&) {
        configureEventLogColumns(m_listView);
    });

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
