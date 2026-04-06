#include "UI/HUD.hpp"
#include "Assets/AssetManager.hpp"

void HUD::init(tgui::Gui& gui, const AssetManager& assets) {
    m_panel = tgui::Panel::create({"100%", "40"});
    m_panel->setPosition({0, 0});
    m_panel->getRenderer()->setBackgroundColor(tgui::Color(40, 40, 40, 220));
    gui.add(m_panel, "HUDPanel");

    m_turnLabel = tgui::Label::create("Turn 1");
    m_turnLabel->setPosition({10, 8});
    m_turnLabel->setTextSize(16);
    m_turnLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_turnLabel);

    m_playerLabel = tgui::Label::create("White's turn");
    m_playerLabel->setPosition({150, 8});
    m_playerLabel->setTextSize(16);
    m_playerLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(m_playerLabel);

    m_goldLabel = tgui::Label::create("Gold: 0");
    m_goldLabel->setPosition({350, 8});
    m_goldLabel->setTextSize(16);
    m_goldLabel->getRenderer()->setTextColor(tgui::Color(255, 215, 0));
    m_panel->add(m_goldLabel);

    auto btnReset = tgui::Button::create("Reset");
    btnReset->setPosition({"&.width - 180", "4"});
    btnReset->setSize({75, 30});
    btnReset->onPress([this]() {
        if (m_onReset) m_onReset();
    });
    m_panel->add(btnReset);

    auto btnPlay = tgui::Button::create("Play");
    btnPlay->setPosition({"&.width - 90", "4"});
    btnPlay->setSize({75, 30});
    btnPlay->onPress([this]() {
        if (m_onPlay) m_onPlay();
    });
    m_panel->add(btnPlay);

    m_panel->setVisible(false);
}

void HUD::show() { if (m_panel) m_panel->setVisible(true); }
void HUD::hide() { if (m_panel) m_panel->setVisible(false); }

void HUD::update(int turnNumber, const std::string& activePlayer, int gold) {
    if (m_turnLabel) m_turnLabel->setText("Turn " + std::to_string(turnNumber));
    if (m_playerLabel) m_playerLabel->setText(activePlayer + "'s turn");
    if (m_goldLabel) m_goldLabel->setText("Gold: " + std::to_string(gold));
}

void HUD::setOnReset(std::function<void()> callback) { m_onReset = std::move(callback); }
void HUD::setOnPlay(std::function<void()> callback) { m_onPlay = std::move(callback); }
