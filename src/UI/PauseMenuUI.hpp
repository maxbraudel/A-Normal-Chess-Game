#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>

class PauseMenuUI {
public:
    void init(tgui::Gui& gui);
    void show();
    void hide();

    void setOnResume(std::function<void()> cb);
    void setOnSave(std::function<void()> cb);
    void setOnQuitToMenu(std::function<void()> cb);

private:
    tgui::Panel::Ptr m_panel;
    std::function<void()> m_onResume;
    std::function<void()> m_onSave;
    std::function<void()> m_onQuitToMenu;
};
