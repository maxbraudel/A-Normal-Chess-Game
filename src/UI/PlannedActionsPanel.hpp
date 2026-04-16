#pragma once

#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>

#include <vector>

#include "UI/InGameViewModel.hpp"

class PlannedActionsPanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show();
    void hide();
    void update(const std::vector<InGamePlannedActionRow>& rows);

private:
    tgui::Panel::Ptr m_panel;
    tgui::ListView::Ptr m_listView;
    std::vector<InGamePlannedActionRow> m_renderedRows;
};