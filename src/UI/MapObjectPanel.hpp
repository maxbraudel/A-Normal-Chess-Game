#pragma once

#include <TGUI/AllWidgets.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>

class MapObject;

class MapObjectPanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show(const MapObject& object);
    void hide();

private:
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_typeLabel;
    tgui::Label::Ptr m_positionLabel;
    tgui::Label::Ptr m_statusLabel;
    tgui::Label::Ptr m_hintLabel;
};