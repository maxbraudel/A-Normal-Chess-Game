#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <vector>

#include "UI/InGameViewModel.hpp"

class EventLogPanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show();
    void hide();
    void update(const std::vector<InGameEventRow>& rows);

private:
    tgui::Panel::Ptr m_panel;
    tgui::ListView::Ptr m_listView;
    std::vector<InGameEventRow> m_renderedRows;
};
