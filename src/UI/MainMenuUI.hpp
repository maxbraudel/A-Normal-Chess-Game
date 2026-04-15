#pragma once

#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>

#include <functional>
#include <string>
#include <vector>

#include "Core/GameSessionConfig.hpp"

class AssetManager;

class MainMenuUI {
public:
    using CreateSaveCallback = std::function<std::string(const GameSessionConfig&)>;

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

private:
    void showMainScreen();
    void updateSaveButtons();
    void updateCreateDialogLabels();
    void openCreateDialog();
    void closeCreateDialog();
    std::string selectedSaveName() const;

    static std::string trimCopy(const std::string& value);
    static bool isValidSaveName(const std::string& value);
    static bool isValidParticipantName(const std::string& value);
    static GameMode parseGameModeId(const tgui::String& id);

    tgui::Panel::Ptr m_panel;
    tgui::Panel::Ptr m_mainBox;
    tgui::Panel::Ptr m_loadBox;
    tgui::Panel::Ptr m_createOverlay;
    tgui::ListBox::Ptr m_saveList;
    tgui::Button::Ptr m_deleteButton;
    tgui::Button::Ptr m_playButton;
    tgui::Label::Ptr m_emptyLabel;
    tgui::EditBox::Ptr m_saveNameEdit;
    tgui::ComboBox::Ptr m_modeCombo;
    tgui::Label::Ptr m_nameOneLabel;
    tgui::Label::Ptr m_nameTwoLabel;
    tgui::EditBox::Ptr m_nameOneEdit;
    tgui::EditBox::Ptr m_nameTwoEdit;
    tgui::Label::Ptr m_createErrorLabel;
    std::vector<SaveSummary> m_saves;

    std::function<void()> m_onLoadSaves;
    std::function<void()> m_onExitGame;
    CreateSaveCallback m_onCreateSave;
    std::function<void(const std::string&)> m_onPlaySave;
    std::function<void(const std::string&)> m_onDeleteSave;
};
