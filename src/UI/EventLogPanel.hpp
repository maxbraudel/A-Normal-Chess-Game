#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>

class EventLog;

class EventLogPanel {
public:
    void init(tgui::Gui& gui);
    void show(const EventLog& log);
    void hide();

private:
    tgui::Panel::Ptr m_panel;
    tgui::ListView::Ptr m_listView;
};
