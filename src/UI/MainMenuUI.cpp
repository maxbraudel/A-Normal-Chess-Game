#include "UI/MainMenuUI.hpp"
#include "Assets/AssetManager.hpp"

void MainMenuUI::init(tgui::Gui& gui, const AssetManager& assets) {
    m_panel = tgui::Panel::create({"100%", "100%"});
    m_panel->getRenderer()->setBackgroundColor(tgui::Color(30, 30, 30, 230));
    gui.add(m_panel, "MainMenuPanel");

    auto title = tgui::Label::create("A NORMAL CHESS GAME");
    title->setPosition({"(&.width - width) / 2", "15%"});
    title->setTextSize(36);
    title->getRenderer()->setTextColor(tgui::Color::White);
    m_panel->add(title);

    auto btnNew = tgui::Button::create("New Game");
    btnNew->setPosition({"(&.width - width) / 2", "40%"});
    btnNew->setSize({200, 50});
    btnNew->onPress([this]() {
        if (m_onNewGame) m_onNewGame();
    });
    m_panel->add(btnNew);

    auto btnContinue = tgui::Button::create("Continue");
    btnContinue->setPosition({"(&.width - width) / 2", "55%"});
    btnContinue->setSize({200, 50});
    btnContinue->onPress([this]() {
        if (m_onContinue) m_onContinue();
    });
    m_panel->add(btnContinue);

    m_panel->setVisible(false);
}

void MainMenuUI::show() { if (m_panel) m_panel->setVisible(true); }
void MainMenuUI::hide() { if (m_panel) m_panel->setVisible(false); }

void MainMenuUI::setOnNewGame(std::function<void()> callback) { m_onNewGame = std::move(callback); }
void MainMenuUI::setOnContinue(std::function<void()> callback) { m_onContinue = std::move(callback); }
