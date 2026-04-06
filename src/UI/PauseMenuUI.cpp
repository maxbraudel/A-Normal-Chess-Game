#include "UI/PauseMenuUI.hpp"

void PauseMenuUI::init(tgui::Gui& gui) {
    // Full-screen dimmed overlay
    m_panel = tgui::Panel::create({"100%", "100%"});
    m_panel->getRenderer()->setBackgroundColor(tgui::Color(0, 0, 0, 160));
    gui.add(m_panel, "PauseMenuPanel");

    // Centered inner box
    auto box = tgui::Panel::create({260, 280});
    box->setPosition({"(&.width - width) / 2", "(&.height - height) / 2"});
    box->getRenderer()->setBackgroundColor(tgui::Color(40, 40, 40, 240));
    box->getRenderer()->setBorders({2, 2, 2, 2});
    box->getRenderer()->setBorderColor(tgui::Color(120, 120, 120));
    m_panel->add(box);

    auto title = tgui::Label::create("PAUSED");
    title->setPosition({"(&.width - width) / 2", 20});
    title->setTextSize(28);
    title->getRenderer()->setTextColor(tgui::Color::White);
    box->add(title);

    auto btnResume = tgui::Button::create("Resume");
    btnResume->setPosition({"(&.width - width) / 2", 90});
    btnResume->setSize({200, 45});
    btnResume->onPress([this]() {
        if (m_onResume) m_onResume();
    });
    box->add(btnResume);

    auto btnSave = tgui::Button::create("Save Game");
    btnSave->setPosition({"(&.width - width) / 2", 150});
    btnSave->setSize({200, 45});
    btnSave->onPress([this]() {
        if (m_onSave) m_onSave();
    });
    box->add(btnSave);

    auto btnQuit = tgui::Button::create("Quit to Menu");
    btnQuit->setPosition({"(&.width - width) / 2", 210});
    btnQuit->setSize({200, 45});
    btnQuit->onPress([this]() {
        if (m_onQuitToMenu) m_onQuitToMenu();
    });
    box->add(btnQuit);

    m_panel->setVisible(false);
}

void PauseMenuUI::show() { if (m_panel) m_panel->setVisible(true); }
void PauseMenuUI::hide() { if (m_panel) m_panel->setVisible(false); }

void PauseMenuUI::setOnResume(std::function<void()> cb)     { m_onResume = std::move(cb); }
void PauseMenuUI::setOnSave(std::function<void()> cb)       { m_onSave = std::move(cb); }
void PauseMenuUI::setOnQuitToMenu(std::function<void()> cb) { m_onQuitToMenu = std::move(cb); }
