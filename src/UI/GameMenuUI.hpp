#pragma once
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <functional>

enum class GameMenuPauseState {
    Paused,
    NotPaused
};

struct GameMenuPresentation {
    GameMenuPauseState pauseState = GameMenuPauseState::Paused;
    bool showSave = true;
};

class GameMenuUI {
public:
    void init(tgui::Gui& gui);
    void show(const GameMenuPresentation& presentation);
    void hide();
    bool isVisible() const;

    void setOnResume(std::function<void()> cb);
    void setOnSave(std::function<void()> cb);
    void setOnQuitToMainMenu(std::function<void()> cb);

private:
    void applyPresentation(const GameMenuPresentation& presentation);

    tgui::Panel::Ptr m_panel;
    tgui::Panel::Ptr m_box;
    tgui::Label::Ptr m_titleLabel;
    tgui::Button::Ptr m_resumeButton;
    tgui::Button::Ptr m_saveButton;
    tgui::Button::Ptr m_quitToMainMenuButton;
    std::function<void()> m_onResume;
    std::function<void()> m_onSave;
    std::function<void()> m_onQuitToMainMenu;
};