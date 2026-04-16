#pragma once
#include "Core/ToolState.hpp"
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>

struct ToolBarPresentation {
    bool selectSelected = true;
    bool selectEnabled = false;
    bool buildSelected = false;
    bool buildEnabled = false;
    bool overviewSelected = true;
    bool overviewEnabled = false;
};

ToolBarPresentation makeToolBarPresentation(ToolState activeTool,
                                            bool canUseToolbar,
                                            bool canOpenBuildPanel,
                                            bool overviewVisible);

class ToolBar {
public:
    void init(tgui::Gui& gui);
    void show();
    void hide();
    bool isVisible() const;

    void setOnSelect(std::function<void()> callback);
    void setOnBuild(std::function<void()> callback);
    void setOnOverview(std::function<void()> callback);
    void applyPresentation(const ToolBarPresentation& presentation);

private:
    tgui::Panel::Ptr m_leftPanel;
    tgui::Panel::Ptr m_rightPanel;
    tgui::Button::Ptr m_selectButton;
    tgui::Button::Ptr m_buildButton;
    tgui::Button::Ptr m_overviewButton;
    std::function<void()> m_onSelect;
    std::function<void()> m_onBuild;
    std::function<void()> m_onOverview;
};
