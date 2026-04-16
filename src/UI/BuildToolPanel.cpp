#include "UI/BuildToolPanel.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Config/GameConfig.hpp"
#include "UI/ActionButtonStyle.hpp"
#include "UI/HUDLayout.hpp"

namespace {

const char* buildingTypeName(BuildingType type) {
    switch (type) {
        case BuildingType::Barracks: return "Barracks";
        case BuildingType::WoodWall: return "Wood Wall";
        case BuildingType::StoneWall: return "Stone Wall";
        case BuildingType::Arena: return "Arena";
        default: return "Unknown";
    }
}

int buildingCost(BuildingType type, const GameConfig& config) {
    switch (type) {
        case BuildingType::Barracks: return config.getBarracksCost();
        case BuildingType::WoodWall: return config.getWoodWallCost();
        case BuildingType::StoneWall: return config.getStoneWallCost();
        case BuildingType::Arena: return config.getArenaCost();
        default: return 0;
    }
}

} // namespace

void BuildToolPanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    auto titleLabel = tgui::Label::create("Construction");
    titleLabel->setPosition({10, 5});
    HUDLayout::styleSidebarTitle(titleLabel);
    m_panel->add(titleLabel);

    auto descriptionLabel = tgui::Label::create("Choose a building for the current turn.");
    descriptionLabel->setPosition({10, 32});
    descriptionLabel->setSize({316, 36});
    descriptionLabel->setAutoSize(false);
    descriptionLabel->setTextSize(13);
    descriptionLabel->getRenderer()->setTextColor(tgui::Color(210, 210, 210));
    m_panel->add(descriptionLabel);

    const std::vector<BuildingType> options = {
        BuildingType::Barracks,
        BuildingType::WoodWall,
        BuildingType::StoneWall,
        BuildingType::Arena
    };

    float y = 80.0f;
    for (BuildingType type : options) {
        auto btn = tgui::Button::create(buildingTypeName(type));
        btn->setPosition({10, y});
        btn->setSize({316, 38});
        btn->setTextSize(15);
        const int typeInt = static_cast<int>(type);
        btn->onPress([this, typeInt]() {
            setSelectedBuildType(static_cast<BuildingType>(typeInt));
            if (m_onSelectBuildType) m_onSelectBuildType(typeInt);
        });
        m_panel->add(btn);
        m_options.push_back({type, btn});
        y += 48.0f;
    }

    m_panel->setVisible(false);
}

void BuildToolPanel::show(const Kingdom& kingdom, const GameConfig& config, bool allowBuild) {
    if (!m_panel) {
        return;
    }

    m_panel->moveToFront();

    for (auto& option : m_options) {
        const int cost = buildingCost(option.type, config);
        std::string label = std::string(buildingTypeName(option.type)) + " - " + std::to_string(cost) + "g";
        if (kingdom.gold < cost) {
            label += " (need gold)";
        }

        ActionButtonStyle::applySelectableState(option.button,
                                                label,
                                                option.type == m_selectedType,
                                                allowBuild);
    }

    m_panel->setVisible(true);
}

void BuildToolPanel::hide() { if (m_panel) m_panel->setVisible(false); }

void BuildToolPanel::setSelectedBuildType(BuildingType type) {
    m_selectedType = type;
}

void BuildToolPanel::setOnSelectBuildType(std::function<void(int)> callback) {
    m_onSelectBuildType = std::move(callback);
}
