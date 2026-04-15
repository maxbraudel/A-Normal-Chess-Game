#include "UI/ToolBar.hpp"
#include "UI/HUDLayout.hpp"

void ToolBar::init(tgui::Gui& gui) {
    m_panel = tgui::Panel::create(HUDLayout::stackSize(2,
                                                       HUDLayout::kToolbarButtonWidth,
                                                       HUDLayout::kComponentGap,
                                                       HUDLayout::kToolbarHeight));
    m_panel->setPosition(HUDLayout::anchorPosition(HUDAnchor::BottomLeft,
                                                   2,
                                                   HUDLayout::kToolbarButtonWidth,
                                                   HUDLayout::kComponentGap,
                                                   HUDLayout::kToolbarHeight));
    HUDLayout::makeTransparentPanel(m_panel);
    gui.add(m_panel, "ToolBarPanel");

    auto btnSelect = tgui::Button::create("Select");
    HUDLayout::styleHudButton(btnSelect, HUDLayout::kToolbarButtonWidth, HUDLayout::kToolbarHeight, 15);
    HUDLayout::placeStackChild(btnSelect, 0, HUDLayout::kToolbarButtonWidth,
                               HUDLayout::kComponentGap, HUDLayout::kToolbarHeight);
    btnSelect->onPress([this]() { if (m_onSelect) m_onSelect(); });
    m_panel->add(btnSelect);

    auto btnBuild = tgui::Button::create("Build");
    HUDLayout::styleHudButton(btnBuild, HUDLayout::kToolbarButtonWidth, HUDLayout::kToolbarHeight, 15);
    HUDLayout::placeStackChild(btnBuild, 1, HUDLayout::kToolbarButtonWidth,
                               HUDLayout::kComponentGap, HUDLayout::kToolbarHeight);
    btnBuild->onPress([this]() { if (m_onBuild) m_onBuild(); });
    m_panel->add(btnBuild);

    m_panel->setVisible(false);
}

void ToolBar::show() { if (m_panel) m_panel->setVisible(true); }
void ToolBar::hide() { if (m_panel) m_panel->setVisible(false); }

void ToolBar::setOnSelect(std::function<void()> callback) { m_onSelect = std::move(callback); }
void ToolBar::setOnBuild(std::function<void()> callback) { m_onBuild = std::move(callback); }
