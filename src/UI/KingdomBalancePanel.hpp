#pragma once

#include <TGUI/AllWidgets.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <array>

#include "UI/InGameViewModel.hpp"

class KingdomBalancePanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show();
    void hide();
    void update(const std::array<KingdomBalanceMetric, 4>& metrics);

private:
    struct MetricWidgets {
        tgui::Panel::Ptr rowPanel;
        tgui::Label::Ptr nameLabel;
        tgui::Label::Ptr whiteValueLabel;
        tgui::Label::Ptr blackValueLabel;
        tgui::ProgressBar::Ptr balanceBar;
    };

    tgui::Panel::Ptr m_panel;
    std::array<MetricWidgets, 4> m_metricWidgets{};
};