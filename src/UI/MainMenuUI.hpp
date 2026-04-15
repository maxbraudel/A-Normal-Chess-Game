#pragma once

#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>

#include <functional>
#include <string>
#include <vector>

#include "Core/GameSessionConfig.hpp"

class AssetManager;

struct JoinMultiplayerRequest {
    std::string host;
    int port = 0;
    std::string password;
};

class MainMenuUI {
public:
    using CreateSaveCallback = std::function<std::string(const GameSessionConfig&)>;
    using JoinMultiplayerCallback = std::function<std::string(const JoinMultiplayerRequest&)>;

    void init(tgui::Gui& gui, const AssetManager& assets);
    void show();
    void hide();
    void showLoadMenu();
    void setSaves(const std::vector<SaveSummary>& saves);

    void setOnLoadSaves(std::function<void()> callback);
    void setOnExitGame(std::function<void()> callback);
    void setOnCreateSave(CreateSaveCallback callback);
    void setOnPlaySave(std::function<void(const std::string&)> callback);
    void setOnDeleteSave(std::function<void(const std::string&)> callback);
    void setOnJoinMultiplayer(JoinMultiplayerCallback callback);

private:
    void showMainScreen();
    void updateSaveButtons();
    void updateCreateDialogLabels();
    void updateMultiplayerControlsVisibility();
    void applyPresetToCreateDialog(GameMode mode);
    void openCreateDialog();
    void closeCreateDialog();
    void openJoinDialog();
    void closeJoinDialog();
    std::string selectedSaveName() const;

    static std::string trimCopy(const std::string& value);
    static bool isValidSaveName(const std::string& value);
    static bool isValidParticipantName(const std::string& value);
    static bool tryParsePort(const std::string& value, int& port);
    static GameMode parseGameModeId(const tgui::String& id);
    static ControllerType parseControllerId(const tgui::String& id);

    tgui::Panel::Ptr m_panel;
    tgui::Panel::Ptr m_mainBox;
    tgui::Panel::Ptr m_loadBox;
    tgui::Panel::Ptr m_createOverlay;
    tgui::Panel::Ptr m_joinOverlay;
    tgui::ListBox::Ptr m_saveList;
    tgui::Button::Ptr m_deleteButton;
    tgui::Button::Ptr m_playButton;
    tgui::Label::Ptr m_emptyLabel;
    tgui::EditBox::Ptr m_saveNameEdit;
    tgui::ComboBox::Ptr m_modeCombo;
    tgui::Label::Ptr m_whiteRoleLabel;
    tgui::CheckBox::Ptr m_multiplayerCheckBox;
    tgui::Label::Ptr m_multiplayerHintLabel;
    tgui::Label::Ptr m_multiplayerPortLabel;
    tgui::EditBox::Ptr m_multiplayerPortEdit;
    tgui::Label::Ptr m_multiplayerPasswordLabel;
    tgui::EditBox::Ptr m_multiplayerPasswordEdit;
    tgui::Label::Ptr m_blackRoleLabel;
    tgui::EditBox::Ptr m_joinHostEdit;
    tgui::EditBox::Ptr m_joinPortEdit;
    tgui::EditBox::Ptr m_joinPasswordEdit;
    tgui::Label::Ptr m_joinErrorLabel;
    tgui::ComboBox::Ptr m_whiteControllerCombo;
    tgui::ComboBox::Ptr m_blackControllerCombo;
    tgui::Label::Ptr m_whiteNameLabel;
    tgui::Label::Ptr m_blackNameLabel;
    tgui::EditBox::Ptr m_whiteNameEdit;
    tgui::EditBox::Ptr m_blackNameEdit;
    tgui::Label::Ptr m_whiteHintLabel;
    JoinMultiplayerCallback m_onJoinMultiplayer;
    tgui::Label::Ptr m_blackHintLabel;
    tgui::Label::Ptr m_createErrorLabel;
    std::vector<SaveSummary> m_saves;

    std::function<void()> m_onLoadSaves;
    std::function<void()> m_onExitGame;
    CreateSaveCallback m_onCreateSave;
    std::function<void(const std::string&)> m_onPlaySave;
    std::function<void(const std::string&)> m_onDeleteSave;
};
