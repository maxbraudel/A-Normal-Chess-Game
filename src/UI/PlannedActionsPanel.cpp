#include "UI/PlannedActionsPanel.hpp"

#include <algorithm>

#include "UI/HUDLayout.hpp"

namespace {

constexpr std::size_t kKindColumnIndex = 0;
constexpr std::size_t kActionColumnIndex = 1;
constexpr std::size_t kDetailColumnIndex = 2;

void configurePlannedActionColumns(const tgui::ListView::Ptr& listView) {
    listView->setColumnWidth(kKindColumnIndex, 70.f);
    listView->setColumnWidth(kActionColumnIndex, 170.f);
    listView->setColumnWidth(kDetailColumnIndex, 190.f);
    listView->setColumnExpanded(kDetailColumnIndex, true);
}

bool sameActionRows(const std::vector<InGamePlannedActionRow>& lhs,
                    const std::vector<InGamePlannedActionRow>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (lhs[index].kindLabel != rhs[index].kindLabel
            || lhs[index].actionLabel != rhs[index].actionLabel
            || lhs[index].detailLabel != rhs[index].detailLabel
            || lhs[index].predicted != rhs[index].predicted) {
            return false;
        }
    }

    return true;
}

} // namespace

void PlannedActionsPanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    auto titleLabel = tgui::Label::create("Actions programmees");
    titleLabel->setPosition({10, 5});
    HUDLayout::styleSidebarTitle(titleLabel);
    m_panel->add(titleLabel);

    m_listView = tgui::ListView::create();
    m_listView->setPosition({8, 34});
    m_listView->setSize({"&.width - 16", "&.height - 42"});
    m_listView->addColumn("Kind", 70.f);
    m_listView->addColumn("Action", 170.f);
    m_listView->addColumn("Details", 190.f);
    configurePlannedActionColumns(m_listView);
    m_listView->setVerticalScrollbarPolicy(tgui::Scrollbar::Policy::Automatic);
    m_listView->setHorizontalScrollbarPolicy(tgui::Scrollbar::Policy::Automatic);
    m_listView->getRenderer()->setBackgroundColor(tgui::Color(24, 24, 24, 210));
    m_listView->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_listView);

    m_listView->onSizeChange([this](const tgui::Vector2f&) {
        configurePlannedActionColumns(m_listView);
    });

    m_panel->setVisible(false);
}

void PlannedActionsPanel::show() {
    if (m_panel) {
        m_panel->setVisible(true);
    }
}

void PlannedActionsPanel::hide() {
    if (m_panel) {
        m_panel->setVisible(false);
    }
}

void PlannedActionsPanel::update(const std::vector<InGamePlannedActionRow>& rows) {
    if (!m_panel || !m_listView) {
        return;
    }

    if (sameActionRows(rows, m_renderedRows)) {
        return;
    }

    const unsigned int previousScroll = m_listView->getVerticalScrollbarValue();
    const unsigned int previousMaxScroll = m_listView->getVerticalScrollbarMaxValue();
    const bool wasAtBottom = previousScroll >= previousMaxScroll;

    m_listView->removeAllItems();
    for (const auto& row : rows) {
        m_listView->addItem({row.kindLabel, row.actionLabel, row.detailLabel});
    }

    const unsigned int newMaxScroll = m_listView->getVerticalScrollbarMaxValue();
    if (wasAtBottom) {
        m_listView->setVerticalScrollbarValue(newMaxScroll);
    } else {
        m_listView->setVerticalScrollbarValue(std::min(previousScroll, newMaxScroll));
    }

    m_renderedRows = rows;
}