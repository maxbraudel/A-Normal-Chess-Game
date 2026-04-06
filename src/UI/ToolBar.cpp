#include "UI/ToolBar.hpp"

void ToolBar::init(tgui::Gui& gui) {
    m_panel = tgui::Panel::create({300, 40});
    m_panel->setPosition({0, "&.height - 40"});
    m_panel->getRenderer()->setBackgroundColor(tgui::Color(40, 40, 40, 220));
    gui.add(m_panel, "ToolBarPanel");

    auto btnSelect = tgui::Button::create("Select");
    btnSelect->setPosition({5, 5});
    btnSelect->setSize({85, 30});
    btnSelect->onPress([this]() { if (m_onSelect) m_onSelect(); });
    m_panel->add(btnSelect);

    auto btnBuild = tgui::Button::create("Build");
    btnBuild->setPosition({100, 5});
    btnBuild->setSize({85, 30});
    btnBuild->onPress([this]() { if (m_onBuild) m_onBuild(); });
    m_panel->add(btnBuild);

    auto btnLog = tgui::Button::create("Log");
    btnLog->setPosition({195, 5});
    btnLog->setSize({85, 30});
    btnLog->onPress([this]() { if (m_onLog) m_onLog(); });
    m_panel->add(btnLog);

    m_panel->setVisible(false);
}

void ToolBar::show() { if (m_panel) m_panel->setVisible(true); }
void ToolBar::hide() { if (m_panel) m_panel->setVisible(false); }

void ToolBar::setOnSelect(std::function<void()> callback) { m_onSelect = std::move(callback); }
void ToolBar::setOnBuild(std::function<void()> callback) { m_onBuild = std::move(callback); }
void ToolBar::setOnLog(std::function<void()> callback) { m_onLog = std::move(callback); }
