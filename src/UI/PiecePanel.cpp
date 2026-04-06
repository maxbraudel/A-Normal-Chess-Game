#include "UI/PiecePanel.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Config/GameConfig.hpp"

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

void PiecePanel::init(tgui::Gui& gui) {
    m_panel = tgui::Panel::create({200, 250});
    m_panel->setPosition({"&.width - 210", "50"});
    m_panel->getRenderer()->setBackgroundColor(tgui::Color(50, 50, 50, 220));
    m_panel->getRenderer()->setBorderColor(tgui::Color::White);
    m_panel->getRenderer()->setBorders({1});
    gui.add(m_panel, "PiecePanel");

    m_typeLabel = tgui::Label::create("Type: ");
    m_typeLabel->setPosition({10, 10});
    m_typeLabel->setTextSize(14);
    m_typeLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_typeLabel);

    m_xpLabel = tgui::Label::create("XP: 0");
    m_xpLabel->setPosition({10, 40});
    m_xpLabel->setTextSize(14);
    m_xpLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_xpLabel);

    m_levelLabel = tgui::Label::create("Level: 1");
    m_levelLabel->setPosition({10, 70});
    m_levelLabel->setTextSize(14);
    m_levelLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_levelLabel);

    m_upgradeBtn = tgui::Button::create("Upgrade");
    m_upgradeBtn->setPosition({10, 110});
    m_upgradeBtn->setSize({180, 35});
    m_upgradeBtn->onPress([this]() {
        if (m_onUpgrade && m_currentPieceId >= 0) m_onUpgrade(m_currentPieceId);
    });
    m_panel->add(m_upgradeBtn);

    m_panel->setVisible(false);
}

void PiecePanel::show(const Piece& piece, const GameConfig& config, bool allowUpgrade) {
    if (!m_panel) return;
    m_currentPieceId = piece.id;
    m_typeLabel->setText("Type: " + pieceTypeName(piece.type));
    m_xpLabel->setText("XP: " + std::to_string(piece.xp));
    m_levelLabel->setText("Level: " + std::to_string(piece.getLevel()));

    bool canUpgrade = piece.canUpgradeTo(
        piece.type == PieceType::Pawn ? PieceType::Knight :
        piece.type == PieceType::Knight ? PieceType::Bishop :
        piece.type == PieceType::Bishop ? PieceType::Rook :
        piece.type == PieceType::Rook ? PieceType::Queen : PieceType::King,
        config);
    m_upgradeBtn->setVisible(canUpgrade);
    m_upgradeBtn->setEnabled(canUpgrade && allowUpgrade);

    m_panel->setVisible(true);
}

void PiecePanel::hide() { if (m_panel) m_panel->setVisible(false); }

void PiecePanel::setOnUpgrade(std::function<void(int)> callback) { m_onUpgrade = std::move(callback); }
