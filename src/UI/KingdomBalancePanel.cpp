#include "UI/KingdomBalancePanel.hpp"

#include <algorithm>

#include "UI/HUDLayout.hpp"

void KingdomBalancePanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    auto titleLabel = tgui::Label::create("Equilibre des royaumes");
    titleLabel->setPosition({10, 8});
    HUDLayout::styleSidebarTitle(titleLabel);
    m_panel->add(titleLabel);

    for (std::size_t index = 0; index < m_metricWidgets.size(); ++index) {
        MetricWidgets widgets;
        const std::string rowHeight = "(&.height - 64) / 4";
        const std::string rowY = "40 + ((&.height - 64) / 4) * " + std::to_string(index);

        widgets.rowPanel = tgui::Panel::create(
            tgui::Layout2d{tgui::Layout{"&.width - 16"}, tgui::Layout{rowHeight}});
        widgets.rowPanel->setPosition(tgui::Layout{"8"}, tgui::Layout{rowY});
        HUDLayout::styleEmbeddedPanel(widgets.rowPanel);
        m_panel->add(widgets.rowPanel);

        widgets.nameLabel = tgui::Label::create("Metric");
        widgets.nameLabel->setPosition({0, 0});
        widgets.nameLabel->setSize({"&.width", 22});
        HUDLayout::styleSidebarBody(widgets.nameLabel, 14);
        widgets.nameLabel->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
        widgets.nameLabel->getRenderer()->setTextColor(HUDLayout::metricColors()[index]);
        widgets.rowPanel->add(widgets.nameLabel);

        widgets.whiteValueLabel = tgui::Label::create("White 0");
        widgets.whiteValueLabel->setPosition({0, 22});
        widgets.whiteValueLabel->setSize({100, 22});
        HUDLayout::styleSidebarBody(widgets.whiteValueLabel, 13);
        widgets.whiteValueLabel->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
        widgets.rowPanel->add(widgets.whiteValueLabel);

        widgets.balanceBar = tgui::ProgressBar::create();
        widgets.balanceBar->setPosition({110, 25});
        widgets.balanceBar->setSize({"&.width - 232", 14});
        widgets.balanceBar->setMinimum(0);
        widgets.balanceBar->setMaximum(100);
        widgets.balanceBar->setValue(50);
        widgets.balanceBar->setText("");
        widgets.balanceBar->getRenderer()->setBackgroundColor(tgui::Color(50, 50, 50, 220));
        widgets.balanceBar->getRenderer()->setFillColor(tgui::Color(220, 220, 220, 240));
        widgets.balanceBar->getRenderer()->setTextColor(tgui::Color::Black);
        widgets.balanceBar->getRenderer()->setBorders(1);
        widgets.balanceBar->getRenderer()->setBorderColor(tgui::Color(140, 140, 140));
        widgets.rowPanel->add(widgets.balanceBar);

        widgets.blackValueLabel = tgui::Label::create("Black 0");
        widgets.blackValueLabel->setPosition({"&.width - 100", 22});
        widgets.blackValueLabel->setSize({100, 22});
        widgets.blackValueLabel->setHorizontalAlignment(tgui::HorizontalAlignment::Right);
        HUDLayout::styleSidebarBody(widgets.blackValueLabel, 13);
        widgets.blackValueLabel->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
        widgets.rowPanel->add(widgets.blackValueLabel);

        m_metricWidgets[index] = widgets;
    }

    m_panel->setVisible(false);
}

void KingdomBalancePanel::show() {
    if (m_panel) {
        m_panel->setVisible(true);
    }
}

void KingdomBalancePanel::hide() {
    if (m_panel) {
        m_panel->setVisible(false);
    }
}

void KingdomBalancePanel::update(const std::array<KingdomBalanceMetric, 4>& metrics) {
    if (!m_panel) {
        return;
    }

    for (std::size_t index = 0; index < m_metricWidgets.size(); ++index) {
        const auto& metric = metrics[index];
        auto& widgets = m_metricWidgets[index];
        const int totalValue = std::max(0, metric.whiteValue) + std::max(0, metric.blackValue);
        const int whiteShare = (totalValue == 0)
            ? 50
            : static_cast<int>((static_cast<long long>(std::max(0, metric.whiteValue)) * 100) / totalValue);

        widgets.nameLabel->setText(metric.label);
        widgets.whiteValueLabel->setText("White " + std::to_string(metric.whiteValue));
        widgets.blackValueLabel->setText("Black " + std::to_string(metric.blackValue));
        widgets.balanceBar->setValue(whiteShare);
        widgets.balanceBar->setText("");
    }
}