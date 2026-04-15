#include "UI/BuildingPanel.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "UI/HUDLayout.hpp"

static std::string buildingTypeName(BuildingType type) {
    switch (type) {
        case BuildingType::Church:    return "Church";
        case BuildingType::Mine:      return "Mine";
        case BuildingType::Farm:      return "Farm";
        case BuildingType::Barracks:  return "Barracks";
        case BuildingType::WoodWall:  return "Wood Wall";
        case BuildingType::StoneWall: return "Stone Wall";
        case BuildingType::Bridge:    return "Bridge";
        case BuildingType::Arena:     return "Arena";
        default: return "Unknown";
    }
}

void BuildingPanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    auto titleLabel = tgui::Label::create("Selection");
    titleLabel->setPosition({10, 10});
    HUDLayout::styleSidebarTitle(titleLabel);
    m_panel->add(titleLabel);

    m_typeLabel = tgui::Label::create("Type: ");
    m_typeLabel->setPosition({10, 50});
    m_typeLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_typeLabel);
    m_panel->add(m_typeLabel);

    m_ownerLabel = tgui::Label::create("Owner: White Kingdom");
    m_ownerLabel->setPosition({10, 80});
    m_ownerLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_ownerLabel);
    m_panel->add(m_ownerLabel);

    m_cellsLabel = tgui::Label::create("Occupied Cells: 0");
    m_cellsLabel->setPosition({10, 110});
    m_cellsLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_cellsLabel);
    m_panel->add(m_cellsLabel);

    m_hpLabel = tgui::Label::create("HP: ");
    m_hpLabel->setPosition({10, 140});
    m_hpLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_hpLabel);
    m_panel->add(m_hpLabel);

    m_statusLabel = tgui::Label::create("Status: ");
    m_statusLabel->setPosition({10, 170});
    m_statusLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_statusLabel);
    m_panel->add(m_statusLabel);

    m_panel->setVisible(false);
}

void BuildingPanel::show(const Building& building) {
    if (!m_panel) return;
    m_typeLabel->setText("Type: " + buildingTypeName(building.type));
    m_ownerLabel->setText("Owner: " + (building.isNeutral ? std::string("Neutral") : kingdomLabel(building.owner)));
    m_cellsLabel->setText("Occupied Cells: " + std::to_string(building.width * building.height));

    // Calculate total HP
    int totalHP = 0;
    int maxHP = 0;
    for (int hp : building.cellHP) {
        totalHP += hp;
        maxHP += (building.type == BuildingType::StoneWall ? 3 : 1);
    }
    m_hpLabel->setText("HP: " + std::to_string(totalHP) + "/" + std::to_string(maxHP));
    m_statusLabel->setText(building.isDestroyed() ? "Status: Destroyed" :
                           building.isNeutral ? "Status: Neutral" : "Status: Active");
    m_panel->setVisible(true);
}

void BuildingPanel::hide() { if (m_panel) m_panel->setVisible(false); }
