#include "UI/PendingConstructionPanel.hpp"

#include "Config/GameConfig.hpp"
#include "UI/HUDLayout.hpp"

namespace {

std::string buildingTypeName(BuildingType type) {
    switch (type) {
        case BuildingType::Church: return "Church";
        case BuildingType::Mine: return "Mine";
        case BuildingType::Farm: return "Farm";
        case BuildingType::Barracks: return "Barracks";
        case BuildingType::WoodWall: return "Wood Wall";
        case BuildingType::StoneWall: return "Stone Wall";
        case BuildingType::Bridge: return "Bridge";
        case BuildingType::Arena: return "Arena";
    }

    return "Building";
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

std::string rotationLabel(int rotationQuarterTurns) {
    const int normalizedRotation = ((rotationQuarterTurns % 4) + 4) % 4;
    return std::to_string(normalizedRotation * 90) + " deg";
}

} // namespace

void PendingConstructionPanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    auto titleLabel = tgui::Label::create("Projet de construction");
    titleLabel->setPosition({10, 10});
    HUDLayout::styleSidebarTitle(titleLabel);
    m_panel->add(titleLabel);

    m_typeLabel = tgui::Label::create("Type: ");
    m_typeLabel->setPosition({10, 50});
    m_typeLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_typeLabel);
    m_panel->add(m_typeLabel);

    m_originLabel = tgui::Label::create("Origin: 0, 0");
    m_originLabel->setPosition({10, 80});
    m_originLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_originLabel);
    m_panel->add(m_originLabel);

    m_footprintLabel = tgui::Label::create("Footprint: 0 x 0");
    m_footprintLabel->setPosition({10, 110});
    m_footprintLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_footprintLabel);
    m_panel->add(m_footprintLabel);

    m_rotationLabel = tgui::Label::create("Rotation: 0 deg");
    m_rotationLabel->setPosition({10, 140});
    m_rotationLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_rotationLabel);
    m_panel->add(m_rotationLabel);

    m_costLabel = tgui::Label::create("Cost: 0g");
    m_costLabel->setPosition({10, 170});
    m_costLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_costLabel);
    m_panel->add(m_costLabel);

    m_removeButton = tgui::Button::create("Retirer le projet de construction");
    m_removeButton->setPosition({10, 222});
    m_removeButton->setSize({316, 36});
    m_removeButton->onPress([this]() {
        if (m_onRemove) {
            m_onRemove(m_currentSelection.type,
                       m_currentSelection.origin,
                       m_currentSelection.rotationQuarterTurns);
        }
    });
    m_panel->add(m_removeButton);

    m_panel->setVisible(false);
}

void PendingConstructionPanel::show(const PendingBuildSelection& selection,
                                    const GameConfig& config,
                                    bool allowRemove) {
    if (!m_panel) {
        return;
    }

    m_currentSelection = selection;
    m_panel->moveToFront();
    m_typeLabel->setText("Type: " + buildingTypeName(selection.type));
    m_originLabel->setText("Origin: " + std::to_string(selection.origin.x)
                           + ", " + std::to_string(selection.origin.y));
    m_footprintLabel->setText("Footprint: " + std::to_string(selection.footprintWidth)
                              + " x " + std::to_string(selection.footprintHeight));
    m_rotationLabel->setText("Rotation: " + rotationLabel(selection.rotationQuarterTurns));
    m_costLabel->setText("Cost: " + std::to_string(buildingCost(selection.type, config)) + "g");
    m_removeButton->setEnabled(allowRemove);
    m_panel->setVisible(true);
}

void PendingConstructionPanel::hide() {
    if (m_panel) {
        m_panel->setVisible(false);
    }
}

void PendingConstructionPanel::setOnRemove(
    std::function<void(BuildingType type, sf::Vector2i origin, int rotationQuarterTurns)> callback) {
    m_onRemove = std::move(callback);
}