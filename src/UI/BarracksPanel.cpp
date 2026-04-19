#include "UI/BarracksPanel.hpp"
#include "Buildings/Building.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/TurnCommand.hpp"
#include "Units/PieceType.hpp"
#include "Config/GameConfig.hpp"
#include "UI/ActionButtonStyle.hpp"
#include "UI/HUDLayout.hpp"

namespace {

std::string pieceTypeName(PieceType type) {
    switch (type) {
        case PieceType::Pawn: return "Pawn";
        case PieceType::Knight: return "Knight";
        case PieceType::Bishop: return "Bishop";
        case PieceType::Rook: return "Rook";
        case PieceType::Queen: return "Queen";
        case PieceType::King: return "King";
    }

    return "Piece";
}

void updateProduceButton(const tgui::Button::Ptr& button,
                         PieceType type,
                         int cost,
                         bool allowProduce,
                         int availableGold,
                         const TurnCommand* pendingProduce) {
    const bool selected = pendingProduce && pendingProduce->produceType == type;
    const bool enabled = allowProduce && (availableGold >= cost || selected);
    ActionButtonStyle::applySelectableState(
        button,
        pieceTypeName(type) + " (" + std::to_string(cost) + "g)",
        selected,
        enabled);
}

    void updateAllProduceButtons(const GameConfig& config,
                     const Kingdom& kingdom,
                     bool allowProduce,
                     const TurnCommand* pendingProduce,
                     const tgui::Button::Ptr& pawnButton,
                     const tgui::Button::Ptr& knightButton,
                     const tgui::Button::Ptr& bishopButton,
                     const tgui::Button::Ptr& rookButton) {
        updateProduceButton(pawnButton,
                PieceType::Pawn,
                config.getRecruitCost(PieceType::Pawn),
                allowProduce,
                kingdom.gold,
                pendingProduce);
        updateProduceButton(knightButton,
                PieceType::Knight,
                config.getRecruitCost(PieceType::Knight),
                allowProduce,
                kingdom.gold,
                pendingProduce);
        updateProduceButton(bishopButton,
                PieceType::Bishop,
                config.getRecruitCost(PieceType::Bishop),
                allowProduce,
                kingdom.gold,
                pendingProduce);
        updateProduceButton(rookButton,
                PieceType::Rook,
                config.getRecruitCost(PieceType::Rook),
                allowProduce,
                kingdom.gold,
                pendingProduce);
    }

} // namespace

void BarracksPanel::init(const tgui::Panel::Ptr& parent) {
    m_panel = tgui::Panel::create({"&.width", "&.height"});
    HUDLayout::styleEmbeddedPanel(m_panel);
    parent->add(m_panel);

    m_titleLabel = tgui::Label::create("");
    HUDLayout::placeSidebarPanelTitle(m_titleLabel);
    m_panel->add(m_titleLabel);

    m_ownerLabel = tgui::Label::create("Owner: White Kingdom");
    HUDLayout::placeSidebarPanelBodyLabel(m_ownerLabel, 0);
    m_panel->add(m_ownerLabel);

    m_cellsLabel = tgui::Label::create("Occupied Cells: 0");
    HUDLayout::placeSidebarPanelBodyLabel(m_cellsLabel, 1);
    m_panel->add(m_cellsLabel);

    m_hpLabel = tgui::Label::create("HP: 0/0");
    HUDLayout::placeSidebarPanelBodyLabel(m_hpLabel, 2);
    m_panel->add(m_hpLabel);

    m_statusLabel = tgui::Label::create("Status: Idle");
    HUDLayout::placeSidebarPanelBodyLabel(m_statusLabel, 3);
    m_panel->add(m_statusLabel);

    m_turnsLabel = tgui::Label::create("");
    m_turnsLabel->setPosition({HUDLayout::kSidebarPanelInset, 168});
    m_turnsLabel->setSize({HUDLayout::kSidebarPanelBodyWidth, HUDLayout::kSidebarPanelBodyHeight});
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

    m_cancelConstructionBtn = tgui::Button::create("Cancel Construction");
    m_cancelConstructionBtn->setPosition({10, 390});
    m_cancelConstructionBtn->setSize({316, 34});
    m_cancelConstructionBtn->onPress([this]() {
        if (m_onCancelConstruction) {
            m_onCancelConstruction(m_currentBarracksId);
        }
    });
    m_panel->add(m_cancelConstructionBtn);

    m_cancelConstructionBtn->setVisible(false);

    m_panel->setVisible(false);
}

void BarracksPanel::show(const Building& barracks,
                         const Kingdom& kingdom,
                         const GameConfig& config,
                         bool allowProduce,
                         bool allowCancelConstruction,
                         const TurnCommand* pendingProduce,
                         const std::string& title) {
    if (!m_panel) return;
    m_panel->moveToFront();
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
    m_currentBarracksId = barracks.id;

    int totalHP = 0;
    const int maxHP = static_cast<int>(barracks.cellHP.size()) * config.getBarracksCellHP();
    for (int hp : barracks.cellHP) {
        totalHP += hp;
    }

    m_ownerLabel->setText("Owner: " + kingdomLabel(barracks.owner));
    m_cellsLabel->setText("Occupied Cells: " + std::to_string(barracks.width * barracks.height));
    m_hpLabel->setText("HP: " + std::to_string(totalHP) + "/" + std::to_string(maxHP));

    const bool canCancelConstruction = barracks.isUnderConstruction() && allowCancelConstruction;
    m_cancelConstructionBtn->setVisible(barracks.isUnderConstruction());
    ActionButtonStyle::applySelectableState(
        m_cancelConstructionBtn,
        "Cancel Construction",
        false,
        canCancelConstruction,
        false);

    if (barracks.isUnderConstruction()) {
        m_statusLabel->setText("Status: Under construction");
        m_turnsLabel->setText("Will complete when the turn is validated");
        updateAllProduceButtons(config,
                                kingdom,
                                false,
                                nullptr,
                                m_producePawnBtn,
                                m_produceKnightBtn,
                                m_produceBishopBtn,
                                m_produceRookBtn);
    } else if (barracks.isProducing) {
        m_statusLabel->setText("Status: Producing...");
        m_turnsLabel->setText("Turns left: " + std::to_string(barracks.turnsRemaining));
        updateAllProduceButtons(config,
                                kingdom,
                                false,
                                nullptr,
                                m_producePawnBtn,
                                m_produceKnightBtn,
                                m_produceBishopBtn,
                                m_produceRookBtn);
    } else {
        if (pendingProduce) {
            m_statusLabel->setText("Status: Production queued");
            m_turnsLabel->setText("Planned: " + pieceTypeName(pendingProduce->produceType));
        } else {
            m_statusLabel->setText("Status: Idle");
            m_turnsLabel->setText("");
        }

        updateAllProduceButtons(config,
                                kingdom,
                                allowProduce,
                                pendingProduce,
                                m_producePawnBtn,
                                m_produceKnightBtn,
                                m_produceBishopBtn,
                                m_produceRookBtn);
    }
    m_panel->setVisible(true);
}

void BarracksPanel::hide() { if (m_panel) m_panel->setVisible(false); }

void BarracksPanel::setOnProduce(std::function<void(int, int)> callback) { m_onProduce = std::move(callback); }

void BarracksPanel::setOnCancelConstruction(std::function<void(int barracksId)> callback) {
    m_onCancelConstruction = std::move(callback);
}
