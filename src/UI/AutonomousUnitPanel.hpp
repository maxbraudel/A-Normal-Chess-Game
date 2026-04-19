#pragma once

#include <TGUI/AllWidgets.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>

#include <string>

struct AutonomousUnit;

class AutonomousUnitPanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show(const AutonomousUnit& unit, const std::string& title = "");
    void hide();

private:
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_titleLabel;
    tgui::Label::Ptr m_ownerLabel;
    tgui::Label::Ptr m_typeLabel;
    tgui::Label::Ptr m_positionLabel;
    tgui::Label::Ptr m_phaseLabel;
    tgui::Label::Ptr m_manifestedTypeLabel;
    tgui::Label::Ptr m_hintLabel;
};