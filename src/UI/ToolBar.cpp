#include "UI/ToolBar.hpp"
#include "UI/HUDLayout.hpp"

void ToolBar::init(tgui::Gui& gui) {
    m_panel = tgui::Panel::create(HUDLayout::stackSize(3));
    m_panel->setPosition(HUDLayout::anchorPosition(HUDAnchor::BottomLeft, 3));
    HUDLayout::makeTransparentPanel(m_panel);
    gui.add(m_panel, "ToolBarPanel");

    auto btnSelect = tgui::Button::create("Select");
    HUDLayout::styleHudButton(btnSelect);
    HUDLayout::placeStackChild(btnSelect, 0);
    btnSelect->onPress([this]() { if (m_onSelect) m_onSelect(); });
    m_panel->add(btnSelect);

    auto btnBuild = tgui::Button::create("Build");
    HUDLayout::styleHudButton(btnBuild);
    HUDLayout::placeStackChild(btnBuild, 1);
    btnBuild->onPress([this]() { if (m_onBuild) m_onBuild(); });
    m_panel->add(btnBuild);

    auto btnLog = tgui::Button::create("Log");
    HUDLayout::styleHudButton(btnLog);
    HUDLayout::placeStackChild(btnLog, 2);
    btnLog->onPress([this]() { if (m_onLog) m_onLog(); });
    m_panel->add(btnLog);

    m_panel->setVisible(false);
}

void ToolBar::show() { if (m_panel) m_panel->setVisible(true); }
void ToolBar::hide() { if (m_panel) m_panel->setVisible(false); }

void ToolBar::setOnSelect(std::function<void()> callback) { m_onSelect = std::move(callback); }
void ToolBar::setOnBuild(std::function<void()> callback) { m_onBuild = std::move(callback); }
void ToolBar::setOnLog(std::function<void()> callback) { m_onLog = std::move(callback); }
