#include "UI/GameMenuUI.hpp"

namespace {

const char* titleForPauseState(GameMenuPauseState pauseState) {
    return pauseState == GameMenuPauseState::Paused ? "Paused" : "Not Paused";
}

} // namespace

void GameMenuUI::init(tgui::Gui& gui) {
    m_panel = tgui::Panel::create({"100%", "100%"});
    m_panel->getRenderer()->setBackgroundColor(tgui::Color(0, 0, 0, 160));
    gui.add(m_panel, "GameMenuPanel");

    m_box = tgui::Panel::create({260, 280});
    m_box->setPosition({"(&.width - width) / 2", "(&.height - height) / 2"});
    m_box->getRenderer()->setBackgroundColor(tgui::Color(40, 40, 40, 240));
    m_box->getRenderer()->setBorders({2, 2, 2, 2});
    m_box->getRenderer()->setBorderColor(tgui::Color(120, 120, 120));
    m_panel->add(m_box);

    m_titleLabel = tgui::Label::create("Paused");
    m_titleLabel->setPosition({"(&.width - width) / 2", 20});
    m_titleLabel->setTextSize(28);
    m_titleLabel->getRenderer()->setTextColor(tgui::Color::White);
    m_box->add(m_titleLabel);

    m_resumeButton = tgui::Button::create("Resume");
    m_resumeButton->setPosition({"(&.width - width) / 2", 90});
    m_resumeButton->setSize({200, 45});
    m_resumeButton->onPress([this]() {
        if (m_onResume) {
            m_onResume();
        }
    });
    m_box->add(m_resumeButton);

    m_saveButton = tgui::Button::create("Save");
    m_saveButton->setPosition({"(&.width - width) / 2", 150});
    m_saveButton->setSize({200, 45});
    m_saveButton->onPress([this]() {
        if (m_onSave) {
            m_onSave();
        }
    });
    m_box->add(m_saveButton);

    m_quitToMainMenuButton = tgui::Button::create("Quit to Main Menu");
    m_quitToMainMenuButton->setPosition({"(&.width - width) / 2", 210});
    m_quitToMainMenuButton->setSize({200, 45});
    m_quitToMainMenuButton->onPress([this]() {
        if (m_onQuitToMainMenu) {
            m_onQuitToMainMenu();
        }
    });
    m_box->add(m_quitToMainMenuButton);

    applyPresentation({});
    m_panel->setVisible(false);
}

void GameMenuUI::show(const GameMenuPresentation& presentation) {
    applyPresentation(presentation);
    if (m_panel) {
        m_panel->setVisible(true);
    }
}

void GameMenuUI::hide() {
    if (m_panel) {
        m_panel->setVisible(false);
    }
}

bool GameMenuUI::isVisible() const {
    return m_panel && m_panel->isVisible();
}

void GameMenuUI::setOnResume(std::function<void()> cb) {
    m_onResume = std::move(cb);
}

void GameMenuUI::setOnSave(std::function<void()> cb) {
    m_onSave = std::move(cb);
}

void GameMenuUI::setOnQuitToMainMenu(std::function<void()> cb) {
    m_onQuitToMainMenu = std::move(cb);
}

void GameMenuUI::applyPresentation(const GameMenuPresentation& presentation) {
    if (m_titleLabel) {
        m_titleLabel->setText(titleForPauseState(presentation.pauseState));
    }

    if (m_box) {
        m_box->setSize({260, presentation.showSave ? 280 : 220});
    }

    if (m_saveButton) {
        m_saveButton->setVisible(presentation.showSave);
    }

    if (m_quitToMainMenuButton) {
        m_quitToMainMenuButton->setPosition({"(&.width - width) / 2", presentation.showSave ? 210 : 150});
    }
}