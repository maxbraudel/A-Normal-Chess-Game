#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>
#include <string>

class AssetManager;
class Kingdom;

class HUD {
public:
    void init(tgui::Gui& gui, const AssetManager& assets);
    void show();
    void hide();
    void update(int turnNumber, const std::string& activePlayer, int gold);

    void setOnReset(std::function<void()> callback);
    void setOnPlay(std::function<void()> callback);

private:
    tgui::Panel::Ptr m_indicatorPanel;
    tgui::Panel::Ptr m_panel;
    tgui::Label::Ptr m_turnLabel;
    tgui::Label::Ptr m_playerLabel;
    tgui::Label::Ptr m_goldLabel;
    std::function<void()> m_onReset;
    std::function<void()> m_onPlay;
};
