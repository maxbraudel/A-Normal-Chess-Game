#pragma once

#include <functional>

#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>

#include "Input/PendingBuildSelection.hpp"

class GameConfig;

class PendingConstructionPanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show(const PendingBuildSelection& selection,
              const GameConfig& config,
              bool allowRemove);
    void hide();

    void setOnRemove(std::function<void(BuildingType type, sf::Vector2i origin, int rotationQuarterTurns)> callback);

private:
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_typeLabel;
    tgui::Label::Ptr m_originLabel;
    tgui::Label::Ptr m_footprintLabel;
    tgui::Label::Ptr m_rotationLabel;
    tgui::Label::Ptr m_costLabel;
    tgui::Button::Ptr m_removeButton;
    std::function<void(BuildingType, sf::Vector2i, int)> m_onRemove;
    PendingBuildSelection m_currentSelection;
};