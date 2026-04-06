#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <string>
#include <functional>

class AssetManager;

class MainMenuUI {
public:
    void init(tgui::Gui& gui, const AssetManager& assets);
    void show();
    void hide();

    void setOnNewGame(std::function<void()> callback);
    void setOnContinue(std::function<void()> callback);

private:
    tgui::Panel::Ptr m_panel;
    std::function<void()> m_onNewGame;
    std::function<void()> m_onContinue;
};
