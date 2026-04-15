#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <array>
#include <functional>
#include <string>

#include "UI/InGameViewModel.hpp"

class AssetManager;

enum class MultiplayerStatusTone {
    Neutral,
    Waiting,
    Connected,
    Error
};

class HUD {
public:
    void init(tgui::Gui& gui, const AssetManager& assets);
    void show();
    void hide();
    void update(const InGameViewModel& model);
    void setMultiplayerStatus(const std::string& text, MultiplayerStatusTone tone);
    void clearMultiplayerStatus();

    void setOnMenu(std::function<void()> callback);
    void setOnResetTurn(std::function<void()> callback);
    void setOnEndTurn(std::function<void()> callback);

private:
    bool m_visible = false;
    tgui::Panel::Ptr m_metricsPanel;
    tgui::Panel::Ptr m_statusPanel;
    tgui::Panel::Ptr m_actionPanel;
    tgui::Panel::Ptr m_networkPanel;
    std::array<tgui::Label::Ptr, 4> m_metricLabels{};
    tgui::Label::Ptr m_statusLabel;
    tgui::Label::Ptr m_networkStatusLabel;
    tgui::Button::Ptr m_menuButton;
    tgui::Button::Ptr m_endTurnButton;
    tgui::Button::Ptr m_resetTurnButton;
    std::function<void()> m_onMenu;
    std::function<void()> m_onResetTurn;
    std::function<void()> m_onEndTurn;
};
