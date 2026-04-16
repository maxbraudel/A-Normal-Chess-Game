#include "UI/ToolBar.hpp"

#include "UI/ActionButtonStyle.hpp"
#include "UI/HUDLayout.hpp"

ToolBarPresentation makeToolBarPresentation(ToolState activeTool,
                                            bool canUseToolbar,
                                            bool canOpenBuildPanel,
                                            bool overviewVisible) {
    ToolBarPresentation presentation;
    presentation.selectSelected = activeTool == ToolState::Select;
    presentation.selectEnabled = canUseToolbar;
    presentation.buildSelected = activeTool == ToolState::Build;
    presentation.buildEnabled = canUseToolbar && canOpenBuildPanel;
    presentation.overviewSelected = overviewVisible;
    presentation.overviewEnabled = canUseToolbar;
    return presentation;
}

void ToolBar::init(tgui::Gui& gui) {
    m_leftPanel = tgui::Panel::create(HUDLayout::stackSize(2,
                                                           HUDLayout::kToolbarButtonWidth,
                                                           HUDLayout::kComponentGap,
                                                           HUDLayout::kToolbarHeight));
    m_leftPanel->setPosition(HUDLayout::anchorPosition(HUDAnchor::BottomLeft,
                                                       2,
                                                       HUDLayout::kToolbarButtonWidth,
                                                       HUDLayout::kComponentGap,
                                                       HUDLayout::kToolbarHeight));
    HUDLayout::makeTransparentPanel(m_leftPanel);
    gui.add(m_leftPanel, "ToolBarLeftPanel");

    m_rightPanel = tgui::Panel::create(HUDLayout::stackSize(1,
                                                            HUDLayout::kToolbarButtonWidth,
                                                            HUDLayout::kComponentGap,
                                                            HUDLayout::kToolbarHeight));
    m_rightPanel->setPosition(HUDLayout::anchorPosition(HUDAnchor::BottomRight,
                                                        1,
                                                        HUDLayout::kToolbarButtonWidth,
                                                        HUDLayout::kComponentGap,
                                                        HUDLayout::kToolbarHeight));
    HUDLayout::makeTransparentPanel(m_rightPanel);
    gui.add(m_rightPanel, "ToolBarRightPanel");

    m_selectButton = tgui::Button::create("Select");
    HUDLayout::styleHudButton(m_selectButton, HUDLayout::kToolbarButtonWidth, HUDLayout::kToolbarHeight, 15);
    HUDLayout::placeStackChild(m_selectButton, 0, HUDLayout::kToolbarButtonWidth,
                               HUDLayout::kComponentGap, HUDLayout::kToolbarHeight);
    m_selectButton->onPress([this]() { if (m_onSelect) m_onSelect(); });
    m_leftPanel->add(m_selectButton);

    m_buildButton = tgui::Button::create("Build");
    HUDLayout::styleHudButton(m_buildButton, HUDLayout::kToolbarButtonWidth, HUDLayout::kToolbarHeight, 15);
    HUDLayout::placeStackChild(m_buildButton, 1, HUDLayout::kToolbarButtonWidth,
                               HUDLayout::kComponentGap, HUDLayout::kToolbarHeight);
    m_buildButton->onPress([this]() { if (m_onBuild) m_onBuild(); });
    m_leftPanel->add(m_buildButton);

    m_overviewButton = tgui::Button::create("Overview");
    HUDLayout::styleHudButton(m_overviewButton, HUDLayout::kToolbarButtonWidth, HUDLayout::kToolbarHeight, 15);
    HUDLayout::placeStackChild(m_overviewButton, 0, HUDLayout::kToolbarButtonWidth,
                               HUDLayout::kComponentGap, HUDLayout::kToolbarHeight);
    m_overviewButton->onPress([this]() { if (m_onOverview) m_onOverview(); });
    m_rightPanel->add(m_overviewButton);

    applyPresentation(makeToolBarPresentation(ToolState::Select, false, false, true));

    m_leftPanel->setVisible(false);
    m_rightPanel->setVisible(false);
}

void ToolBar::show() {
    if (m_leftPanel) m_leftPanel->setVisible(true);
    if (m_rightPanel) m_rightPanel->setVisible(true);
}

void ToolBar::hide() {
    if (m_leftPanel) m_leftPanel->setVisible(false);
    if (m_rightPanel) m_rightPanel->setVisible(false);
}

bool ToolBar::isVisible() const {
    return (m_leftPanel && m_leftPanel->isVisible())
        || (m_rightPanel && m_rightPanel->isVisible());
}

void ToolBar::setOnSelect(std::function<void()> callback) { m_onSelect = std::move(callback); }
void ToolBar::setOnBuild(std::function<void()> callback) { m_onBuild = std::move(callback); }
void ToolBar::setOnOverview(std::function<void()> callback) { m_onOverview = std::move(callback); }

void ToolBar::applyPresentation(const ToolBarPresentation& presentation) {
    ActionButtonStyle::applySelectableState(
        m_selectButton,
        "Select",
        presentation.selectSelected,
        presentation.selectEnabled,
        false);
    ActionButtonStyle::applySelectableState(
        m_buildButton,
        "Build",
        presentation.buildSelected,
        presentation.buildEnabled,
        false);
    ActionButtonStyle::applySelectableState(
        m_overviewButton,
        "Overview",
        presentation.overviewSelected,
        presentation.overviewEnabled,
        false);
}
