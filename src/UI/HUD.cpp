#include "UI/HUD.hpp"
#include "Assets/AssetManager.hpp"
#include "UI/HUDLayout.hpp"

void HUD::init(tgui::Gui& gui, const AssetManager& assets) {
    (void) assets;

    m_indicatorPanel = tgui::Panel::create(HUDLayout::stackSize(3));
    m_indicatorPanel->setPosition(HUDLayout::anchorPosition(HUDAnchor::TopLeft, 3));
    HUDLayout::makeTransparentPanel(m_indicatorPanel);
    gui.add(m_indicatorPanel, "HUDIndicatorsPanel");

    m_panel = tgui::Panel::create(HUDLayout::stackSize(2));
    m_panel->setPosition(HUDLayout::anchorPosition(HUDAnchor::TopRight, 2));
    HUDLayout::makeTransparentPanel(m_panel);
    gui.add(m_panel, "HUDPanel");

    m_turnLabel = tgui::Label::create("Turn 1");
    HUDLayout::styleHudIndicator(m_turnLabel, tgui::Color::White);
    HUDLayout::placeStackChild(m_turnLabel, 0);
    m_indicatorPanel->add(m_turnLabel);

    m_playerLabel = tgui::Label::create("White's turn");
    HUDLayout::styleHudIndicator(m_playerLabel, tgui::Color::White);
    HUDLayout::placeStackChild(m_playerLabel, 1);
    m_indicatorPanel->add(m_playerLabel);

    m_goldLabel = tgui::Label::create("Gold: 0");
    HUDLayout::styleHudIndicator(m_goldLabel, tgui::Color(255, 215, 0));
    HUDLayout::placeStackChild(m_goldLabel, 2);
    m_indicatorPanel->add(m_goldLabel);

    auto btnReset = tgui::Button::create("Reset");
    HUDLayout::styleHudButton(btnReset);
    HUDLayout::placeStackChild(btnReset, 0);
    btnReset->onPress([this]() {
        if (m_onReset) m_onReset();
    });
    m_panel->add(btnReset);

    auto btnPlay = tgui::Button::create("Play");
    HUDLayout::styleHudButton(btnPlay);
    HUDLayout::placeStackChild(btnPlay, 1);
    btnPlay->onPress([this]() {
        if (m_onPlay) m_onPlay();
    });
    m_panel->add(btnPlay);

    m_indicatorPanel->setVisible(false);
    m_panel->setVisible(false);
}

void HUD::show() {
    if (m_indicatorPanel) m_indicatorPanel->setVisible(true);
    if (m_panel) m_panel->setVisible(true);
}

void HUD::hide() {
    if (m_indicatorPanel) m_indicatorPanel->setVisible(false);
    if (m_panel) m_panel->setVisible(false);
}

void HUD::update(int turnNumber, TurnPhase turnPhase, int gold) {
    const char* turnText = (turnPhase == TurnPhase::WhiteTurn) ? "Player turn" : "Black turn";
    if (m_turnLabel) m_turnLabel->setText("Turn " + std::to_string(turnNumber));
    if (m_playerLabel) m_playerLabel->setText(turnText);
    if (m_goldLabel) m_goldLabel->setText("Gold: " + std::to_string(gold));
}

void HUD::setOnReset(std::function<void()> callback) { m_onReset = std::move(callback); }
void HUD::setOnPlay(std::function<void()> callback) { m_onPlay = std::move(callback); }
