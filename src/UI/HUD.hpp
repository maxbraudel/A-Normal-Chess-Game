#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>
#include <string>
#include "Core/TurnPhase.hpp"

class AssetManager;
class Kingdom;

class HUD {
public:
    void init(tgui::Gui& gui, const AssetManager& assets);
    void show();
    void hide();
    void update(int turnNumber, const std::string& activePlayerText, int gold, bool allowCommands);

    void setOnReset(std::function<void()> callback);
    void setOnPlay(std::function<void()> callback);

private:
    tgui::Panel::Ptr m_indicatorPanel;
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_turnLabel;
    tgui::Label::Ptr m_playerLabel;
    tgui::Label::Ptr m_goldLabel;
    tgui::Button::Ptr m_resetButton;
    tgui::Button::Ptr m_playButton;
    std::function<void()> m_onReset;
    std::function<void()> m_onPlay;
};
