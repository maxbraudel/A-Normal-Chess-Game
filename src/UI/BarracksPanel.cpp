#include "UI/BarracksPanel.hpp"
#include "Buildings/Building.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/PieceType.hpp"
#include "Config/GameConfig.hpp"

void BarracksPanel::init(tgui::Gui& gui) {
    m_panel = tgui::Panel::create({200, 300});
    m_panel->setPosition({"&.width - 210", "50"});
    m_panel->getRenderer()->setBackgroundColor(tgui::Color(50, 50, 50, 220));
    m_panel->getRenderer()->setBorderColor(tgui::Color::White);
    m_panel->getRenderer()->setBorders({1});
    gui.add(m_panel, "BarracksPanel");

    auto titleLabel = tgui::Label::create("Barracks");
    titleLabel->setPosition({10, 5});
    titleLabel->setTextSize(16);
    titleLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(titleLabel);

    m_statusLabel = tgui::Label::create("Status: Idle");
    m_statusLabel->setPosition({10, 30});
    m_statusLabel->setTextSize(13);
    m_statusLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_statusLabel);

    m_turnsLabel = tgui::Label::create("");
    m_turnsLabel->setPosition({10, 55});
    m_turnsLabel->setTextSize(13);
    m_turnsLabel->getRenderer()->setTextColor(tgui::Color(200, 200, 200));
    m_panel->add(m_turnsLabel);

    m_producePawnBtn = tgui::Button::create("Produce Pawn");
    m_producePawnBtn->setPosition({10, 90});
    m_producePawnBtn->setSize({180, 30});
    m_producePawnBtn->onPress([this]() {
        if (m_onProduce) m_onProduce(m_currentBarracksId, static_cast<int>(PieceType::Pawn));
    });
    m_panel->add(m_producePawnBtn);

    m_produceKnightBtn = tgui::Button::create("Produce Knight");
    m_produceKnightBtn->setPosition({10, 130});
    m_produceKnightBtn->setSize({180, 30});
    m_produceKnightBtn->onPress([this]() {
        if (m_onProduce) m_onProduce(m_currentBarracksId, static_cast<int>(PieceType::Knight));
    });
    m_panel->add(m_produceKnightBtn);

    m_produceBishopBtn = tgui::Button::create("Produce Bishop");
    m_produceBishopBtn->setPosition({10, 170});
    m_produceBishopBtn->setSize({180, 30});
    m_produceBishopBtn->onPress([this]() {
        if (m_onProduce) m_onProduce(m_currentBarracksId, static_cast<int>(PieceType::Bishop));
    });
    m_panel->add(m_produceBishopBtn);

    m_produceRookBtn = tgui::Button::create("Produce Rook");
    m_produceRookBtn->setPosition({10, 210});
    m_produceRookBtn->setSize({180, 30});
    m_produceRookBtn->onPress([this]() {
        if (m_onProduce) m_onProduce(m_currentBarracksId, static_cast<int>(PieceType::Rook));
    });
    m_panel->add(m_produceRookBtn);

    m_panel->setVisible(false);
}

void BarracksPanel::show(const Building& barracks, const Kingdom& kingdom, const GameConfig& config,
                         bool allowProduce) {
    if (!m_panel) return;
    m_currentBarracksId = barracks.id;

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
