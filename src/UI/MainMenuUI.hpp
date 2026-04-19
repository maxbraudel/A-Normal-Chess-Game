#pragma once

#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>

#include <functional>
#include <string>
#include <vector>

#include "Core/GameSessionConfig.hpp"
#include "Runtime/SessionFormRequest.hpp"

class AssetManager;

struct JoinMultiplayerRequest {
    std::string host;
    int port = 0;
    std::string password;
};

class MainMenuUI {
public:
    using SubmitSessionCallback = std::function<std::string(const SessionFormRequest&)>;
    using JoinMultiplayerCallback = std::function<std::string(const JoinMultiplayerRequest&)>;

    void init(tgui::Gui& gui, const AssetManager& assets);
    void show();
    void hide();
    void showLoadMenu();
    void setSaves(const std::vector<SaveSummary>& saves);

    void setOnLoadSaves(std::function<void()> callback);
    void setOnExitGame(std::function<void()> callback);
    void setOnCreateSave(SubmitSessionCallback callback);
    void setOnPlaySave(std::function<void(const std::string&)> callback);
    void setOnDeleteSave(std::function<void(const std::string&)> callback);
    void setOnEditSave(SubmitSessionCallback callback);
    void setOnJoinMultiplayer(JoinMultiplayerCallback callback);

private:
    enum class SessionDialogMode {
        Create,
        Edit
    };

    void showMainScreen();
    void updateSaveButtons();
    void updateSessionDialogLabels();
    void updateMultiplayerControlsVisibility();
    void openCreateDialog();
    void openEditDialog();
    void closeSessionDialog();
    void openJoinDialog();
    void closeJoinDialog();
    std::string selectedSaveName() const;
    const SaveSummary* selectedSaveSummary() const;
    void submitSessionDialog();

    static std::string trimCopy(const std::string& value);
    static bool isValidSaveName(const std::string& value);
    static bool isValidParticipantName(const std::string& value);
    static bool tryParsePort(const std::string& value, int& port);

    tgui::Panel::Ptr m_panel;
    tgui::Panel::Ptr m_mainBox;
    tgui::Panel::Ptr m_loadBox;
    tgui::Panel::Ptr m_createOverlay;
    tgui::Panel::Ptr m_joinOverlay;
    tgui::ListBox::Ptr m_saveList;
    tgui::Button::Ptr m_deleteButton;
    tgui::Button::Ptr m_editButton;
    tgui::Button::Ptr m_playButton;
    tgui::Label::Ptr m_emptyLabel;
    tgui::EditBox::Ptr m_saveNameEdit;
    tgui::Label::Ptr m_whiteRoleLabel;
    tgui::CheckBox::Ptr m_multiplayerCheckBox;
    tgui::CheckBox::Ptr m_tacticalGridCheckBox;
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
    tgui::Label::Ptr m_whiteNameLabel;
    tgui::Label::Ptr m_blackNameLabel;
    tgui::EditBox::Ptr m_whiteNameEdit;
    tgui::EditBox::Ptr m_blackNameEdit;
    tgui::Label::Ptr m_whiteHintLabel;
    JoinMultiplayerCallback m_onJoinMultiplayer;
    tgui::Label::Ptr m_blackHintLabel;
    tgui::Label::Ptr m_createErrorLabel;
    tgui::Label::Ptr m_createTitleLabel;
    tgui::Button::Ptr m_createConfirmButton;
    std::vector<SaveSummary> m_saves;
    SessionDialogMode m_sessionDialogMode = SessionDialogMode::Create;
    std::string m_originalSaveNameForEdit;

    std::function<void()> m_onLoadSaves;
    std::function<void()> m_onExitGame;
    SubmitSessionCallback m_onCreateSave;
    std::function<void(const std::string&)> m_onPlaySave;
    std::function<void(const std::string&)> m_onDeleteSave;
    SubmitSessionCallback m_onEditSave;
};
