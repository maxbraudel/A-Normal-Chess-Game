#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>

class Building;

class BuildingPanel {
public:
    void init(tgui::Gui& gui);
    void show(const Building& building);
    void hide();

private:
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_typeLabel;
    tgui::Label::Ptr m_hpLabel;
    tgui::Label::Ptr m_statusLabel;
};
