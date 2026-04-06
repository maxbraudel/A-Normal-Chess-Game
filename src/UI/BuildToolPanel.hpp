#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>

class Kingdom;
class GameConfig;

class BuildToolPanel {
public:
    void init(tgui::Gui& gui);
    void show(const Kingdom& kingdom, const GameConfig& config);
    void hide();

    void setOnSelectBuildType(std::function<void(int buildingType)> callback);

private:
    tgui::Panel::Ptr m_panel;
    std::function<void(int)> m_onSelectBuildType;
};
