#include "UI/PiecePanel.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Config/GameConfig.hpp"
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

void PiecePanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    auto titleLabel = tgui::Label::create("Selection");
    titleLabel->setPosition({10, 10});
    HUDLayout::styleSidebarTitle(titleLabel);
    m_panel->add(titleLabel);

    m_ownerLabel = tgui::Label::create("Owner: White Kingdom");
    m_ownerLabel->setPosition({10, 50});
    m_ownerLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_ownerLabel);
    m_panel->add(m_ownerLabel);

    m_positionLabel = tgui::Label::create("Cell: 0, 0");
    m_positionLabel->setPosition({10, 80});
    m_positionLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_positionLabel);
    m_panel->add(m_positionLabel);

    m_typeLabel = tgui::Label::create("Type: ");
    m_typeLabel->setPosition({10, 110});
    m_typeLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_typeLabel);
    m_panel->add(m_typeLabel);

    m_xpLabel = tgui::Label::create("XP: 0");
    m_xpLabel->setPosition({10, 140});
    m_xpLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_xpLabel);
    m_panel->add(m_xpLabel);

    m_levelLabel = tgui::Label::create("Level: 1");
    m_levelLabel->setPosition({10, 170});
    m_levelLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_levelLabel);
    m_panel->add(m_levelLabel);

    m_primaryUpgradeBtn = tgui::Button::create("Upgrade");
    m_primaryUpgradeBtn->setPosition({10, 220});
    m_primaryUpgradeBtn->setSize({316, 36});
    m_primaryUpgradeBtn->onPress([this]() {
        if (m_onUpgrade && m_currentPieceId >= 0) {
            m_onUpgrade(m_currentPieceId, m_primaryUpgradeTarget);
        }
    });
    m_panel->add(m_primaryUpgradeBtn);

    m_secondaryUpgradeBtn = tgui::Button::create("Upgrade");
    m_secondaryUpgradeBtn->setPosition({10, 264});
    m_secondaryUpgradeBtn->setSize({316, 36});
    m_secondaryUpgradeBtn->onPress([this]() {
        if (m_onUpgrade && m_currentPieceId >= 0) {
            m_onUpgrade(m_currentPieceId, m_secondaryUpgradeTarget);
        }
    });
    m_panel->add(m_secondaryUpgradeBtn);

    m_panel->setVisible(false);
}

void PiecePanel::show(const Piece& piece, const GameConfig& config, bool allowUpgrade) {
    if (!m_panel) return;
    m_panel->moveToFront();
    m_currentPieceId = piece.id;
    m_ownerLabel->setText("Owner: " + kingdomLabel(piece.kingdom));
    m_positionLabel->setText("Cell: " + std::to_string(piece.position.x) + ", "
                             + std::to_string(piece.position.y));
    m_typeLabel->setText("Type: " + pieceTypeName(piece.type));
    m_xpLabel->setText("XP: " + std::to_string(piece.xp));
    m_levelLabel->setText("Level: " + std::to_string(piece.getLevel()));

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
    m_primaryUpgradeBtn->setEnabled(hasPrimaryUpgrade && allowUpgrade);
    if (hasPrimaryUpgrade) {
        m_primaryUpgradeBtn->setText("Upgrade to " + pieceTypeName(upgradeTargets[0]));
        m_primaryUpgradeTarget = static_cast<int>(upgradeTargets[0]);
    }

    m_secondaryUpgradeBtn->setVisible(hasSecondaryUpgrade);
    m_secondaryUpgradeBtn->setEnabled(hasSecondaryUpgrade && allowUpgrade);
    if (hasSecondaryUpgrade) {
        m_secondaryUpgradeBtn->setText("Upgrade to " + pieceTypeName(upgradeTargets[1]));
        m_secondaryUpgradeTarget = static_cast<int>(upgradeTargets[1]);
    }

    m_panel->setVisible(true);
}

void PiecePanel::hide() { if (m_panel) m_panel->setVisible(false); }

void PiecePanel::setOnUpgrade(std::function<void(int, int)> callback) { m_onUpgrade = std::move(callback); }
