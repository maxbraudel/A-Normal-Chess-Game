#include "UI/PiecePanel.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Systems/TurnCommand.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Config/GameConfig.hpp"
#include "UI/ActionButtonStyle.hpp"
#include "UI/HUDLayout.hpp"

#include <vector>

static std::string pieceTypeName(PieceType type) {
    switch (type) {
        case PieceType::Pawn:   return "Pawn";
        case PieceType::Knight: return "Knight";
        case PieceType::Bishop: return "Bishop";
        case PieceType::Rook:   return "Rook";
        case PieceType::Queen:  return "Queen";
        case PieceType::King:   return "King";
        default: return "Unknown";
    }
}

static std::string moveCostText(PieceType type, const GameConfig& config) {
    const int moveCost = config.getMovePointCost(type);
    return "Movement Cost: " + std::to_string(moveCost)
        + (moveCost == 1 ? " point" : " points");
}

static std::string upkeepText(PieceType type, const GameConfig& config) {
    return "Upkeep: " + std::to_string(config.getPieceUpkeepCost(type)) + " gold/turn";
}

void PiecePanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    m_titleLabel = tgui::Label::create("");
    HUDLayout::placeSidebarPanelTitle(m_titleLabel);
    m_panel->add(m_titleLabel);

    m_ownerLabel = tgui::Label::create("Owner: White Kingdom");
    HUDLayout::placeSidebarPanelBodyLabel(m_ownerLabel, 0);
    m_panel->add(m_ownerLabel);

    m_positionLabel = tgui::Label::create("Cell: 0, 0");
    HUDLayout::placeSidebarPanelBodyLabel(m_positionLabel, 1);
    m_panel->add(m_positionLabel);

    m_typeLabel = tgui::Label::create("Type: ");
    HUDLayout::placeSidebarPanelBodyLabel(m_typeLabel, 2);
    m_panel->add(m_typeLabel);

    m_xpLabel = tgui::Label::create("XP: 0");
    HUDLayout::placeSidebarPanelBodyLabel(m_xpLabel, 3);
    m_panel->add(m_xpLabel);

    m_levelLabel = tgui::Label::create("Level: 1");
    HUDLayout::placeSidebarPanelBodyLabel(m_levelLabel, 4);
    m_panel->add(m_levelLabel);

    m_moveCostLabel = tgui::Label::create("Movement Cost: 0 points");
    HUDLayout::placeSidebarPanelBodyLabel(m_moveCostLabel, 5);
    m_panel->add(m_moveCostLabel);

    m_upkeepLabel = tgui::Label::create("Upkeep: 0 gold/turn");
    HUDLayout::placeSidebarPanelBodyLabel(m_upkeepLabel, 6);
    m_panel->add(m_upkeepLabel);

    m_primaryUpgradeBtn = tgui::Button::create("Upgrade");
    m_primaryUpgradeBtn->setPosition({10, 274});
    m_primaryUpgradeBtn->setSize({316, 36});
    m_primaryUpgradeBtn->onPress([this]() {
        if (m_onUpgrade && m_currentPieceId >= 0) {
            m_onUpgrade(m_currentPieceId, m_primaryUpgradeTarget);
        }
    });
    m_panel->add(m_primaryUpgradeBtn);

    m_secondaryUpgradeBtn = tgui::Button::create("Upgrade");
    m_secondaryUpgradeBtn->setPosition({10, 318});
    m_secondaryUpgradeBtn->setSize({316, 36});
    m_secondaryUpgradeBtn->onPress([this]() {
        if (m_onUpgrade && m_currentPieceId >= 0) {
            m_onUpgrade(m_currentPieceId, m_secondaryUpgradeTarget);
        }
    });
    m_panel->add(m_secondaryUpgradeBtn);

    m_disbandButton = tgui::Button::create("Tuer la piece");
    m_disbandButton->setPosition({10, 362});
    m_disbandButton->setSize({316, 36});
    m_disbandButton->onPress([this]() {
        if (m_onDisband && m_currentPieceId >= 0) {
            m_onDisband(m_currentPieceId);
        }
    });
    m_panel->add(m_disbandButton);

    m_panel->setVisible(false);
}

void PiecePanel::show(const Piece& piece,
                      const GameConfig& config,
                      bool allowUpgrade,
                      bool allowDisband,
                      const TurnCommand* pendingUpgrade,
                      const TurnCommand* pendingDisband,
                      const std::string& title) {
    if (!m_panel) return;
    m_panel->moveToFront();
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
    m_currentPieceId = piece.id;
    m_ownerLabel->setText("Owner: " + kingdomLabel(piece.kingdom));
    m_positionLabel->setText("Cell: " + std::to_string(piece.position.x) + ", "
                             + std::to_string(piece.position.y));
    m_typeLabel->setText("Type: " + pieceTypeName(piece.type));
    m_xpLabel->setText("XP: " + std::to_string(piece.xp));
    m_levelLabel->setText("Level: " + std::to_string(piece.getLevel()));
    m_moveCostLabel->setText(moveCostText(piece.type, config));
    m_upkeepLabel->setText(upkeepText(piece.type, config));

    std::vector<PieceType> upgradeTargets;
    if (piece.type == PieceType::Pawn) {
        if (piece.canUpgradeTo(PieceType::Knight, config)) {
            upgradeTargets.push_back(PieceType::Knight);
        }
        if (piece.canUpgradeTo(PieceType::Bishop, config)) {
            upgradeTargets.push_back(PieceType::Bishop);
        }
    } else if (piece.type == PieceType::Knight || piece.type == PieceType::Bishop) {
        if (piece.canUpgradeTo(PieceType::Rook, config)) {
            upgradeTargets.push_back(PieceType::Rook);
        }
    }

    const bool hasPrimaryUpgrade = !upgradeTargets.empty();
    const bool hasSecondaryUpgrade = upgradeTargets.size() > 1;

    m_primaryUpgradeBtn->setVisible(hasPrimaryUpgrade);
    if (hasPrimaryUpgrade) {
        const std::string label = "Upgrade to " + pieceTypeName(upgradeTargets[0]);
        const bool selected = pendingUpgrade
            && pendingUpgrade->upgradeTarget == upgradeTargets[0];
        ActionButtonStyle::applySelectableState(m_primaryUpgradeBtn,
                                                label,
                                                selected,
                                                allowUpgrade);
        m_primaryUpgradeTarget = static_cast<int>(upgradeTargets[0]);
    }

    m_secondaryUpgradeBtn->setVisible(hasSecondaryUpgrade);
    if (hasSecondaryUpgrade) {
        const std::string label = "Upgrade to " + pieceTypeName(upgradeTargets[1]);
        const bool selected = pendingUpgrade
            && pendingUpgrade->upgradeTarget == upgradeTargets[1];
        ActionButtonStyle::applySelectableState(m_secondaryUpgradeBtn,
                                                label,
                                                selected,
                                                allowUpgrade);
        m_secondaryUpgradeTarget = static_cast<int>(upgradeTargets[1]);
    }

    const bool canShowDisband = piece.type != PieceType::King;
    m_disbandButton->setVisible(canShowDisband);
    if (canShowDisband) {
        ActionButtonStyle::applySelectableState(m_disbandButton,
                                                "Tuer la piece",
                                                pendingDisband != nullptr,
                                                allowDisband);
    }

    m_panel->setVisible(true);
}

void PiecePanel::hide() { if (m_panel) m_panel->setVisible(false); }

void PiecePanel::setOnUpgrade(std::function<void(int, int)> callback) { m_onUpgrade = std::move(callback); }
void PiecePanel::setOnDisband(std::function<void(int)> callback) { m_onDisband = std::move(callback); }
