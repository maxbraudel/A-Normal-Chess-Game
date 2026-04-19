#include "UI/BuildingPanel.hpp"
#include "UI/ActionButtonStyle.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "UI/HUDLayout.hpp"

namespace {

std::string formatIncomeLabel(const std::string& prefix, int income) {
    return prefix + ": +" + std::to_string(income) + " gold/turn";
}

int defaultCellHPForDisplay(const Building& building) {
    switch (building.type) {
        case BuildingType::StoneWall:
            return 3;

        default:
            return 1;
    }
}

int currentDurabilityHPForDisplay(const Building& building) {
    const int remainingCellsToDestroy = std::max(
        0,
        building.effectiveDestroyedCellsRequired() - building.destroyedCellCount());
    if (remainingCellsToDestroy <= 0) {
        return 0;
    }

    std::vector<int> survivingCellHP;
    survivingCellHP.reserve(building.cellHP.size());
    for (int hp : building.cellHP) {
        if (hp > 0) {
            survivingCellHP.push_back(hp);
        }
    }

    std::sort(survivingCellHP.begin(), survivingCellHP.end());

    int durabilityHP = 0;
    for (int index = 0;
         index < remainingCellsToDestroy && index < static_cast<int>(survivingCellHP.size());
         ++index) {
        durabilityHP += survivingCellHP[static_cast<std::size_t>(index)];
    }

    return durabilityHP;
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

    m_titleLabel = tgui::Label::create("");
    HUDLayout::placeSidebarPanelTitle(m_titleLabel);
    m_panel->add(m_titleLabel);

    m_typeLabel = tgui::Label::create("Type: ");
    HUDLayout::placeSidebarPanelBodyLabel(m_typeLabel, 0);
    m_panel->add(m_typeLabel);

    m_ownerLabel = tgui::Label::create("Owner: White Kingdom");
    HUDLayout::placeSidebarPanelBodyLabel(m_ownerLabel, 1);
    m_panel->add(m_ownerLabel);

    m_cellsLabel = tgui::Label::create("Occupied Cells: 0");
    HUDLayout::placeSidebarPanelBodyLabel(m_cellsLabel, 2);
    m_panel->add(m_cellsLabel);

    m_hpLabel = tgui::Label::create("Durability: ");
    HUDLayout::placeSidebarPanelBodyLabel(m_hpLabel, 3);
    m_panel->add(m_hpLabel);

    m_statusLabel = tgui::Label::create("Status: Completed");
    HUDLayout::placeSidebarPanelBodyLabel(m_statusLabel, 4);
    m_panel->add(m_statusLabel);

    m_cancelConstructionBtn = tgui::Button::create("Cancel Construction");
    m_cancelConstructionBtn->setPosition({10, 202});
    m_cancelConstructionBtn->setSize({316, 34});
    m_cancelConstructionBtn->onPress([this]() {
        if (m_onCancelConstruction) {
            m_onCancelConstruction(m_currentBuildingId);
        }
    });
    m_panel->add(m_cancelConstructionBtn);

    m_resourceSectionLabel = tgui::Label::create("Resource Income");
    m_resourceSectionLabel->setPosition({10, 252});
    HUDLayout::styleSidebarTitle(m_resourceSectionLabel);
    m_panel->add(m_resourceSectionLabel);

    m_whiteIncomeLabel = tgui::Label::create("White: +0 gold/turn");
    m_whiteIncomeLabel->setPosition({10, 284});
    m_whiteIncomeLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_whiteIncomeLabel);
    m_panel->add(m_whiteIncomeLabel);

    m_blackIncomeLabel = tgui::Label::create("Black: +0 gold/turn");
    m_blackIncomeLabel->setPosition({10, 314});
    m_blackIncomeLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_blackIncomeLabel);
    m_panel->add(m_blackIncomeLabel);

    m_cancelConstructionBtn->setVisible(false);
    m_resourceSectionLabel->setVisible(false);
    m_whiteIncomeLabel->setVisible(false);
    m_blackIncomeLabel->setVisible(false);

    m_panel->setVisible(false);
}

void BuildingPanel::show(const Building& building,
                         bool allowCancelConstruction,
                         const std::optional<ResourceIncomeBreakdown>& resourceIncome,
                         const std::optional<PublicBuildingOccupationState>& publicOccupation,
                         const std::string& title) {
    if (!m_panel) return;
    m_panel->moveToFront();
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
    m_currentBuildingId = building.id;
    m_typeLabel->setText("Type: " + buildingTypeName(building.type));
    const std::string ownerLabel = building.isPublic()
        ? publicOccupationOwnerLabel(publicOccupation.value_or(PublicBuildingOccupationState::Unoccupied))
        : (building.isNeutral ? std::string("Neutral") : kingdomLabel(building.owner));
    m_ownerLabel->setText("Owner: " + ownerLabel);
    m_cellsLabel->setText("Occupied Cells: " + std::to_string(building.width * building.height));

    if (building.isPublic()) {
        m_hpLabel->setText("Durability: Permanent");
    } else {
        const int maxDurabilityHP = building.effectiveDestroyedCellsRequired()
            * defaultCellHPForDisplay(building);
        const int currentDurabilityHP = currentDurabilityHPForDisplay(building);
        m_hpLabel->setText(
            "Durability: "
            + std::to_string(currentDurabilityHP)
            + "/"
            + std::to_string(maxDurabilityHP));
    }

    if (building.isUnderConstruction()) {
        m_statusLabel->setText("Status: Under construction");
    } else if (building.isDestroyed()) {
        m_statusLabel->setText("Status: Destroyed");
    } else {
        m_statusLabel->setText("Status: Completed");
    }

    const bool canCancelConstruction = building.isUnderConstruction() && allowCancelConstruction;
    m_cancelConstructionBtn->setVisible(building.isUnderConstruction());
    ActionButtonStyle::applySelectableState(
        m_cancelConstructionBtn,
        "Cancel Construction",
        false,
        canCancelConstruction,
        false);

    const bool showResourceIncome = building.hasActiveGameplayEffects()
        && resourceIncome.has_value()
        && resourceIncome->isResourceBuilding;
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

void BuildingPanel::setOnCancelConstruction(std::function<void(int buildingId)> callback) {
    m_onCancelConstruction = std::move(callback);
}
