#include "UI/BarracksPanel.hpp"
#include "Buildings/Building.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/PieceType.hpp"
#include "Config/GameConfig.hpp"
#include "UI/HUDLayout.hpp"

void BarracksPanel::init(const tgui::Panel::Ptr& parent) {
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

    m_cellsLabel = tgui::Label::create("Occupied Cells: 0");
    m_cellsLabel->setPosition({10, 80});
    m_cellsLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_cellsLabel);
    m_panel->add(m_cellsLabel);

    m_hpLabel = tgui::Label::create("HP: 0/0");
    m_hpLabel->setPosition({10, 110});
    m_hpLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_hpLabel);
    m_panel->add(m_hpLabel);

    m_statusLabel = tgui::Label::create("Status: Idle");
    m_statusLabel->setPosition({10, 140});
    m_statusLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_statusLabel);
    m_panel->add(m_statusLabel);

    m_turnsLabel = tgui::Label::create("");
    m_turnsLabel->setPosition({10, 168});
    m_turnsLabel->setSize({316, 22});
    HUDLayout::styleSidebarBody(m_turnsLabel, 13);
    m_panel->add(m_turnsLabel);

    m_producePawnBtn = tgui::Button::create("Produce Pawn");
    m_producePawnBtn->setPosition({10, 214});
    m_producePawnBtn->setSize({316, 34});
    m_producePawnBtn->onPress([this]() {
        if (m_onProduce) m_onProduce(m_currentBarracksId, static_cast<int>(PieceType::Pawn));
    });
    m_panel->add(m_producePawnBtn);

    m_produceKnightBtn = tgui::Button::create("Produce Knight");
    m_produceKnightBtn->setPosition({10, 256});
    m_produceKnightBtn->setSize({316, 34});
    m_produceKnightBtn->onPress([this]() {
        if (m_onProduce) m_onProduce(m_currentBarracksId, static_cast<int>(PieceType::Knight));
    });
    m_panel->add(m_produceKnightBtn);

    m_produceBishopBtn = tgui::Button::create("Produce Bishop");
    m_produceBishopBtn->setPosition({10, 298});
    m_produceBishopBtn->setSize({316, 34});
    m_produceBishopBtn->onPress([this]() {
        if (m_onProduce) m_onProduce(m_currentBarracksId, static_cast<int>(PieceType::Bishop));
    });
    m_panel->add(m_produceBishopBtn);

    m_produceRookBtn = tgui::Button::create("Produce Rook");
    m_produceRookBtn->setPosition({10, 340});
    m_produceRookBtn->setSize({316, 34});
    m_produceRookBtn->onPress([this]() {
        if (m_onProduce) m_onProduce(m_currentBarracksId, static_cast<int>(PieceType::Rook));
    });
    m_panel->add(m_produceRookBtn);

    m_panel->setVisible(false);
}

void BarracksPanel::show(const Building& barracks, const Kingdom& kingdom, const GameConfig& config,
                         bool allowProduce) {
    if (!m_panel) return;
    m_panel->moveToFront();
    m_currentBarracksId = barracks.id;

    int totalHP = 0;
    const int maxHP = static_cast<int>(barracks.cellHP.size()) * config.getBarracksCellHP();
    for (int hp : barracks.cellHP) {
        totalHP += hp;
    }

    m_ownerLabel->setText("Owner: " + kingdomLabel(barracks.owner));
    m_cellsLabel->setText("Occupied Cells: " + std::to_string(barracks.width * barracks.height));
    m_hpLabel->setText("HP: " + std::to_string(totalHP) + "/" + std::to_string(maxHP));

    if (barracks.isProducing) {
        m_statusLabel->setText("Status: Producing...");
        m_turnsLabel->setText("Turns left: " + std::to_string(barracks.turnsRemaining));
        m_producePawnBtn->setEnabled(false);
        m_produceKnightBtn->setEnabled(false);
        m_produceBishopBtn->setEnabled(false);
        m_produceRookBtn->setEnabled(false);
    } else {
        m_statusLabel->setText("Status: Idle");
        m_turnsLabel->setText("");
        int pawnCost = config.getRecruitCost(PieceType::Pawn);
        int knightCost = config.getRecruitCost(PieceType::Knight);
        int bishopCost = config.getRecruitCost(PieceType::Bishop);
        int rookCost = config.getRecruitCost(PieceType::Rook);
        m_producePawnBtn->setEnabled(allowProduce && kingdom.gold >= pawnCost);
        m_produceKnightBtn->setEnabled(allowProduce && kingdom.gold >= knightCost);
        m_produceBishopBtn->setEnabled(allowProduce && kingdom.gold >= bishopCost);
        m_produceRookBtn->setEnabled(allowProduce && kingdom.gold >= rookCost);
        m_producePawnBtn->setText("Pawn (" + std::to_string(pawnCost) + "g)");
        m_produceKnightBtn->setText("Knight (" + std::to_string(knightCost) + "g)");
        m_produceBishopBtn->setText("Bishop (" + std::to_string(bishopCost) + "g)");
        m_produceRookBtn->setText("Rook (" + std::to_string(rookCost) + "g)");
    }
    m_panel->setVisible(true);
}

void BarracksPanel::hide() { if (m_panel) m_panel->setVisible(false); }

void BarracksPanel::setOnProduce(std::function<void(int, int)> callback) { m_onProduce = std::move(callback); }
