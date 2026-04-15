#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>

class ToolBar {
public:
    void init(tgui::Gui& gui);
    void show();
    void hide();

    void setOnSelect(std::function<void()> callback);
    void setOnBuild(std::function<void()> callback);

private:
    tgui::Panel::Ptr m_panel;
    std::function<void()> m_onSelect;
    std::function<void()> m_onBuild;
};
