#include "UI/BuildingPanel.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "UI/HUDLayout.hpp"

namespace {

std::string formatIncomeLabel(const std::string& prefix, int income) {
    return prefix + ": +" + std::to_string(income) + " gold/turn";
}

std::string publicOccupationOwnerLabel(PublicBuildingOccupationState state) {
    switch (state) {
        case PublicBuildingOccupationState::WhiteOccupied:
            return kingdomLabel(KingdomId::White);
        case PublicBuildingOccupationState::BlackOccupied:
            return kingdomLabel(KingdomId::Black);
        case PublicBuildingOccupationState::Contested:
            return "Disputed";
        case PublicBuildingOccupationState::Unoccupied:
        default:
            return "Neutral";
    }
}

} // namespace

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

    m_resourceSectionLabel = tgui::Label::create("Resource Income");
    m_resourceSectionLabel->setPosition({10, 180});
    HUDLayout::styleSidebarTitle(m_resourceSectionLabel);
    m_panel->add(m_resourceSectionLabel);

    m_whiteIncomeLabel = tgui::Label::create("White: +0 gold/turn");
    m_whiteIncomeLabel->setPosition({10, 212});
    m_whiteIncomeLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_whiteIncomeLabel);
    m_panel->add(m_whiteIncomeLabel);

    m_blackIncomeLabel = tgui::Label::create("Black: +0 gold/turn");
    m_blackIncomeLabel->setPosition({10, 242});
    m_blackIncomeLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_blackIncomeLabel);
    m_panel->add(m_blackIncomeLabel);

    m_resourceSectionLabel->setVisible(false);
    m_whiteIncomeLabel->setVisible(false);
    m_blackIncomeLabel->setVisible(false);

    m_panel->setVisible(false);
}

void BuildingPanel::show(const Building& building,
                         const std::optional<ResourceIncomeBreakdown>& resourceIncome,
                         const std::optional<PublicBuildingOccupationState>& publicOccupation) {
    if (!m_panel) return;
    m_panel->moveToFront();
    m_typeLabel->setText("Type: " + buildingTypeName(building.type));
    const std::string ownerLabel = building.isPublic()
        ? publicOccupationOwnerLabel(publicOccupation.value_or(PublicBuildingOccupationState::Unoccupied))
        : (building.isNeutral ? std::string("Neutral") : kingdomLabel(building.owner));
    m_ownerLabel->setText("Owner: " + ownerLabel);
    m_cellsLabel->setText("Occupied Cells: " + std::to_string(building.width * building.height));

    // Calculate total HP
    int totalHP = 0;
    int maxHP = 0;
    for (int hp : building.cellHP) {
        totalHP += hp;
        maxHP += (building.type == BuildingType::StoneWall ? 3 : 1);
    }
    m_hpLabel->setText("HP: " + std::to_string(totalHP) + "/" + std::to_string(maxHP));

    const bool showResourceIncome = resourceIncome.has_value() && resourceIncome->isResourceBuilding;
    m_resourceSectionLabel->setVisible(showResourceIncome);
    m_whiteIncomeLabel->setVisible(showResourceIncome);
    m_blackIncomeLabel->setVisible(showResourceIncome);
    if (showResourceIncome) {
        m_whiteIncomeLabel->setText(formatIncomeLabel("White", resourceIncome->whiteIncome));
        m_blackIncomeLabel->setText(formatIncomeLabel("Black", resourceIncome->blackIncome));
    }

    m_panel->setVisible(true);
}

void BuildingPanel::hide() { if (m_panel) m_panel->setVisible(false); }
