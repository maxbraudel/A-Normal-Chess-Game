#pragma once

#include <TGUI/AllWidgets.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>

struct Cell;

class CellPanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show(const Cell& cell);
    void hide();

private:
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_positionLabel;
    tgui::Label::Ptr m_terrainLabel;
    tgui::Label::Ptr m_zoneLabel;
    tgui::Label::Ptr m_statusLabel;
};