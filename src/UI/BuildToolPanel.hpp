#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>
#include <vector>

#include "Buildings/BuildingType.hpp"

class Kingdom;
class GameConfig;

class BuildToolPanel {
public:
    void init(const tgui::Panel::Ptr& parent);
    void show(const Kingdom& kingdom, const GameConfig& config, bool allowBuild);
    void hide();
    void setSelectedBuildType(BuildingType type);

    void setOnSelectBuildType(std::function<void(int buildingType)> callback);

private:
    struct BuildOptionWidgets {
        BuildingType type;
        tgui::Button::Ptr button;
    };

    tgui::Panel::Ptr m_panel;
    std::vector<BuildOptionWidgets> m_options;
    BuildingType m_selectedType = BuildingType::Barracks;
    std::function<void(int)> m_onSelectBuildType;
};
