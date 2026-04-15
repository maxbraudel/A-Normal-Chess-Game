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

    const int metricBaseY = 42;
    const int metricGap = 54;

    for (std::size_t index = 0; index < m_metricWidgets.size(); ++index) {
        MetricWidgets widgets;
        const int y = metricBaseY + static_cast<int>(index) * metricGap;

        widgets.nameLabel = tgui::Label::create("Metric");
        widgets.nameLabel->setPosition({10, y});
        widgets.nameLabel->setSize({"&.width - 20", 18});
        HUDLayout::styleSidebarBody(widgets.nameLabel, 14);
        widgets.nameLabel->getRenderer()->setTextColor(HUDLayout::metricColors()[index]);
        m_panel->add(widgets.nameLabel);

        widgets.whiteValueLabel = tgui::Label::create("White 0");
        widgets.whiteValueLabel->setPosition({10, y + 20});
        widgets.whiteValueLabel->setSize({100, 20});
        HUDLayout::styleSidebarBody(widgets.whiteValueLabel, 13);
        m_panel->add(widgets.whiteValueLabel);

        widgets.balanceBar = tgui::ProgressBar::create();
        widgets.balanceBar->setPosition({116, y + 22});
        widgets.balanceBar->setSize({"&.width - 232", 16});
        widgets.balanceBar->setMinimum(0);
        widgets.balanceBar->setMaximum(100);
        widgets.balanceBar->setValue(50);
        widgets.balanceBar->setText("");
        widgets.balanceBar->getRenderer()->setBackgroundColor(tgui::Color(50, 50, 50, 220));
        widgets.balanceBar->getRenderer()->setFillColor(tgui::Color(220, 220, 220, 240));
        widgets.balanceBar->getRenderer()->setTextColor(tgui::Color::Black);
        widgets.balanceBar->getRenderer()->setBorders(1);
        widgets.balanceBar->getRenderer()->setBorderColor(tgui::Color(140, 140, 140));
        m_panel->add(widgets.balanceBar);

        widgets.blackValueLabel = tgui::Label::create("Black 0");
    widgets.blackValueLabel->setPosition({"&.width - 110", y + 20});
    widgets.blackValueLabel->setSize({100, 20});
        widgets.blackValueLabel->setHorizontalAlignment(tgui::HorizontalAlignment::Right);
        HUDLayout::styleSidebarBody(widgets.blackValueLabel, 13);
        m_panel->add(widgets.blackValueLabel);

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