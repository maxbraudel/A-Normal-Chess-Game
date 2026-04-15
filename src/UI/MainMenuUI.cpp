#include "UI/MainMenuUI.hpp"

#include "Assets/AssetManager.hpp"
#include "Multiplayer/PasswordUtils.hpp"

#include <cctype>
#include <limits>

namespace {
std::string buildSaveLabel(const SaveSummary& save) {
    const auto mode = gameModeFromParticipants(save.kingdoms);
    const auto& white = kingdomParticipantConfig(save.kingdoms, KingdomId::White);
    const auto& black = kingdomParticipantConfig(save.kingdoms, KingdomId::Black);
    std::string label = save.saveName + " - " + gameModeLabel(mode)
        + " | White: " + white.participantName
        + " | Black: " + black.participantName;
    if (save.multiplayer.enabled) {
        label += " | LAN";
    }
    return label;
}
}

void MainMenuUI::init(tgui::Gui& gui, const AssetManager& assets) {
    (void) assets;

    m_panel = tgui::Panel::create({"100%", "100%"});
    m_panel->getRenderer()->setBackgroundColor(tgui::Color(30, 30, 30, 230));
    gui.add(m_panel, "MainMenuPanel");

    m_mainBox = tgui::Panel::create({"100%", "100%"});
    m_mainBox->getRenderer()->setBackgroundColor(tgui::Color::Transparent);
    m_panel->add(m_mainBox);

    auto title = tgui::Label::create("A NORMAL CHESS GAME");
    title->setPosition({"(&.width - width) / 2", "15%"});
    title->setTextSize(36);
    title->getRenderer()->setTextColor(tgui::Color::White);
    m_mainBox->add(title);

    auto btnLoad = tgui::Button::create("Load Save");
    btnLoad->setPosition({"(&.width - width) / 2", "39%"});
    btnLoad->setSize({220, 52});
    btnLoad->onPress([this]() {
        if (m_onLoadSaves) m_onLoadSaves();
        showLoadMenu();
    });
    m_mainBox->add(btnLoad);

    auto btnJoin = tgui::Button::create("Join Multiplayer");
    btnJoin->setPosition({"(&.width - width) / 2", "51%"});
    btnJoin->setSize({220, 52});
    btnJoin->onPress([this]() {
        openJoinDialog();
    });
    m_mainBox->add(btnJoin);

    auto btnExit = tgui::Button::create("Exit Game");
    btnExit->setPosition({"(&.width - width) / 2", "63%"});
    btnExit->setSize({220, 52});
    btnExit->onPress([this]() {
        if (m_onExitGame) m_onExitGame();
    });
    m_mainBox->add(btnExit);

    m_loadBox = tgui::Panel::create({"100%", "100%"});
    m_loadBox->getRenderer()->setBackgroundColor(tgui::Color::Transparent);
    m_loadBox->setVisible(false);
    m_panel->add(m_loadBox);

    auto backButton = tgui::Button::create("Back");
    backButton->setPosition({40, 32});
    backButton->setSize({110, 42});
    backButton->onPress([this]() {
        showMainScreen();
    });
    m_loadBox->add(backButton);

    auto loadTitle = tgui::Label::create("Load Save");
    loadTitle->setPosition({"(&.width - width) / 2", "11%"});
    loadTitle->setTextSize(30);
    loadTitle->getRenderer()->setTextColor(tgui::Color::White);
    m_loadBox->add(loadTitle);

    m_deleteButton = tgui::Button::create("Delete");
    m_deleteButton->setPosition({"(&.width - 620) / 2", "24%"});
    m_deleteButton->setSize({190, 44});
    m_deleteButton->setEnabled(false);
    m_deleteButton->onPress([this]() {
        const std::string saveName = selectedSaveName();
        if (!saveName.empty() && m_onDeleteSave) {
            m_onDeleteSave(saveName);
        }
    });
    m_loadBox->add(m_deleteButton);

    auto newSaveButton = tgui::Button::create("New Save");
    newSaveButton->setPosition({"(&.width + 620) / 2 - width", "24%"});
    newSaveButton->setSize({190, 44});
    newSaveButton->onPress([this]() {
        openCreateDialog();
    });
    m_loadBox->add(newSaveButton);

    m_saveList = tgui::ListBox::create();
    m_saveList->setPosition({"(&.width - width) / 2", "33%"});
    m_saveList->setSize({620, 255});
    m_saveList->onItemSelect([this]() {
        updateSaveButtons();
    });
    m_loadBox->add(m_saveList);

    m_emptyLabel = tgui::Label::create("No saves available.");
    m_emptyLabel->setPosition({"(&.width - width) / 2", "47%"});
    m_emptyLabel->setTextSize(20);
    m_emptyLabel->getRenderer()->setTextColor(tgui::Color(220, 220, 220));
    m_loadBox->add(m_emptyLabel);

    m_playButton = tgui::Button::create("Play Save");
    m_playButton->setPosition({"(&.width - width) / 2", "74%"});
    m_playButton->setSize({240, 48});
    m_playButton->setEnabled(false);
    m_playButton->onPress([this]() {
        const std::string saveName = selectedSaveName();
        if (!saveName.empty() && m_onPlaySave) {
            m_onPlaySave(saveName);
        }
    });
    m_loadBox->add(m_playButton);

    m_createOverlay = tgui::Panel::create({"100%", "100%"});
    m_createOverlay->getRenderer()->setBackgroundColor(tgui::Color(0, 0, 0, 170));
    m_createOverlay->setVisible(false);
    m_panel->add(m_createOverlay);

    auto dialog = tgui::Panel::create({560, 720});
    dialog->setPosition({"(&.parent.width - width) / 2", "(&.parent.height - height) / 2"});
    dialog->getRenderer()->setBackgroundColor(tgui::Color(46, 46, 46, 245));
    dialog->getRenderer()->setBorderColor(tgui::Color(120, 120, 120));
    dialog->getRenderer()->setBorders(2);
    m_createOverlay->add(dialog);

    auto createTitle = tgui::Label::create("Create New Save");
    createTitle->setPosition({"(&.width - width) / 2", 18});
    createTitle->setTextSize(26);
    createTitle->getRenderer()->setTextColor(tgui::Color::White);
    dialog->add(createTitle);

    auto saveNameLabel = tgui::Label::create("Save name");
    saveNameLabel->setPosition({36, 78});
    saveNameLabel->setTextSize(18);
    saveNameLabel->getRenderer()->setTextColor(tgui::Color::White);
    dialog->add(saveNameLabel);

    m_saveNameEdit = tgui::EditBox::create();
    m_saveNameEdit->setPosition({36, 106});
    m_saveNameEdit->setSize({488, 34});
    dialog->add(m_saveNameEdit);

    auto modeLabel = tgui::Label::create("Session preset");
    modeLabel->setPosition({36, 154});
    modeLabel->setTextSize(18);
    modeLabel->getRenderer()->setTextColor(tgui::Color::White);
    dialog->add(modeLabel);

    m_modeCombo = tgui::ComboBox::create();
    m_modeCombo->setPosition({36, 182});
    m_modeCombo->setSize({488, 34});
    m_modeCombo->addItem("Human vs AI", "human_ai");
    m_modeCombo->addItem("Human vs Human", "human_human");
    m_modeCombo->addItem("AI vs AI", "ai_ai");
    m_modeCombo->setSelectedItemById("human_ai");
    m_modeCombo->onItemSelect([this]() {
        applyPresetToCreateDialog(parseGameModeId(m_modeCombo->getSelectedItemId()));
    });
    dialog->add(m_modeCombo);

    m_whiteRoleLabel = tgui::Label::create("White Kingdom");
    m_whiteRoleLabel->setPosition({36, 236});
    m_whiteRoleLabel->setTextSize(20);
    m_whiteRoleLabel->getRenderer()->setTextColor(tgui::Color::White);
    dialog->add(m_whiteRoleLabel);

    auto whiteControllerLabel = tgui::Label::create("Controller");
    whiteControllerLabel->setPosition({36, 266});
    whiteControllerLabel->setTextSize(16);
    whiteControllerLabel->getRenderer()->setTextColor(tgui::Color(220, 220, 220));
    dialog->add(whiteControllerLabel);

    m_whiteControllerCombo = tgui::ComboBox::create();
    m_whiteControllerCombo->setPosition({36, 290});
    m_whiteControllerCombo->setSize({180, 32});
    m_whiteControllerCombo->addItem("Human", "human");
    m_whiteControllerCombo->addItem("AI", "ai");
    m_whiteControllerCombo->onItemSelect([this]() {
        updateCreateDialogLabels();
    });
    dialog->add(m_whiteControllerCombo);

    m_whiteNameLabel = tgui::Label::create("Player name");
    m_whiteNameLabel->setPosition({236, 266});
    m_whiteNameLabel->setTextSize(16);
    m_whiteNameLabel->getRenderer()->setTextColor(tgui::Color(220, 220, 220));
    dialog->add(m_whiteNameLabel);

    m_whiteNameEdit = tgui::EditBox::create();
    m_whiteNameEdit->setPosition({236, 290});
    m_whiteNameEdit->setSize({288, 32});
    dialog->add(m_whiteNameEdit);

    m_whiteHintLabel = tgui::Label::create("White starts first and spawns on the left.");
    m_whiteHintLabel->setPosition({36, 330});
    m_whiteHintLabel->setTextSize(14);
    m_whiteHintLabel->getRenderer()->setTextColor(tgui::Color(180, 180, 180));
    dialog->add(m_whiteHintLabel);

    m_blackRoleLabel = tgui::Label::create("Black Kingdom");
    m_blackRoleLabel->setPosition({36, 370});
    m_blackRoleLabel->setTextSize(20);
    m_blackRoleLabel->getRenderer()->setTextColor(tgui::Color::White);
    dialog->add(m_blackRoleLabel);

    auto blackControllerLabel = tgui::Label::create("Controller");
    blackControllerLabel->setPosition({36, 400});
    blackControllerLabel->setTextSize(16);
    blackControllerLabel->getRenderer()->setTextColor(tgui::Color(220, 220, 220));
    dialog->add(blackControllerLabel);

    m_blackControllerCombo = tgui::ComboBox::create();
    m_blackControllerCombo->setPosition({36, 424});
    m_blackControllerCombo->setSize({180, 32});
    m_blackControllerCombo->addItem("Human", "human");
    m_blackControllerCombo->addItem("AI", "ai");
    m_blackControllerCombo->onItemSelect([this]() {
        updateCreateDialogLabels();
    });
    dialog->add(m_blackControllerCombo);

    m_blackNameLabel = tgui::Label::create("Player name");
    m_blackNameLabel->setPosition({236, 400});
    m_blackNameLabel->setTextSize(16);
    m_blackNameLabel->getRenderer()->setTextColor(tgui::Color(220, 220, 220));
    dialog->add(m_blackNameLabel);

    m_blackNameEdit = tgui::EditBox::create();
    m_blackNameEdit->setPosition({236, 424});
    m_blackNameEdit->setSize({288, 32});
    dialog->add(m_blackNameEdit);

    m_blackHintLabel = tgui::Label::create("Black plays second and spawns on the right.");
    m_blackHintLabel->setPosition({36, 464});
    m_blackHintLabel->setTextSize(14);
    m_blackHintLabel->getRenderer()->setTextColor(tgui::Color(180, 180, 180));
    dialog->add(m_blackHintLabel);

    m_multiplayerCheckBox = tgui::CheckBox::create();
    m_multiplayerCheckBox->setPosition({36, 500});
    m_multiplayerCheckBox->setText("Multiplayer");
    m_multiplayerCheckBox->setTextSize(18);
    m_multiplayerCheckBox->onChange([this](bool) {
        updateMultiplayerControlsVisibility();
        if (m_createErrorLabel) m_createErrorLabel->setText("");
    });
    dialog->add(m_multiplayerCheckBox);

    m_multiplayerHintLabel = tgui::Label::create("White hosts the LAN game. Black joins from another machine.");
    m_multiplayerHintLabel->setPosition({36, 532});
    m_multiplayerHintLabel->setTextSize(14);
    m_multiplayerHintLabel->getRenderer()->setTextColor(tgui::Color(180, 180, 180));
    dialog->add(m_multiplayerHintLabel);

    m_multiplayerPortLabel = tgui::Label::create("Server port");
    m_multiplayerPortLabel->setPosition({36, 564});
    m_multiplayerPortLabel->setTextSize(16);
    m_multiplayerPortLabel->getRenderer()->setTextColor(tgui::Color(220, 220, 220));
    dialog->add(m_multiplayerPortLabel);

    m_multiplayerPortEdit = tgui::EditBox::create();
    m_multiplayerPortEdit->setPosition({36, 588});
    m_multiplayerPortEdit->setSize({180, 32});
    m_multiplayerPortEdit->setInputValidator(tgui::EditBox::Validator::UInt);
    dialog->add(m_multiplayerPortEdit);

    m_multiplayerPasswordLabel = tgui::Label::create("Server password");
    m_multiplayerPasswordLabel->setPosition({236, 564});
    m_multiplayerPasswordLabel->setTextSize(16);
    m_multiplayerPasswordLabel->getRenderer()->setTextColor(tgui::Color(220, 220, 220));
    dialog->add(m_multiplayerPasswordLabel);

    m_multiplayerPasswordEdit = tgui::EditBox::create();
    m_multiplayerPasswordEdit->setPosition({236, 588});
    m_multiplayerPasswordEdit->setSize({288, 32});
    m_multiplayerPasswordEdit->setPasswordCharacter('*');
    dialog->add(m_multiplayerPasswordEdit);

    m_createErrorLabel = tgui::Label::create("");
    m_createErrorLabel->setPosition({36, 656});
    m_createErrorLabel->setSize({280, 44});
    m_createErrorLabel->setAutoSize(false);
    m_createErrorLabel->setTextSize(15);
    m_createErrorLabel->getRenderer()->setTextColor(tgui::Color(255, 120, 120));
    dialog->add(m_createErrorLabel);

    auto cancelButton = tgui::Button::create("Cancel");
    cancelButton->setPosition({332, 656});
    cancelButton->setSize({92, 30});
    cancelButton->onPress([this]() {
        closeCreateDialog();
    });
    dialog->add(cancelButton);

    auto createButton = tgui::Button::create("Create");
    createButton->setPosition({432, 656});
    createButton->setSize({92, 30});
    createButton->onPress([this]() {
        GameSessionConfig session;
        session.saveName = trimCopy(m_saveNameEdit->getText().toStdString());
        session.kingdoms[kingdomIndex(KingdomId::White)] = makeKingdomParticipantConfig(
            KingdomId::White,
            parseControllerId(m_whiteControllerCombo->getSelectedItemId()),
            trimCopy(m_whiteNameEdit->getText().toStdString()));
        session.kingdoms[kingdomIndex(KingdomId::Black)] = makeKingdomParticipantConfig(
            KingdomId::Black,
            parseControllerId(m_blackControllerCombo->getSelectedItemId()),
            trimCopy(m_blackNameEdit->getText().toStdString()));

        if (session.saveName.empty()) {
            m_createErrorLabel->setText("Save name is required.");
            return;
        }
        if (!isValidSaveName(session.saveName)) {
            m_createErrorLabel->setText("Save name contains invalid characters.");
            return;
        }
        if (participantNameFor(session, KingdomId::White).empty()
            || participantNameFor(session, KingdomId::Black).empty()) {
            m_createErrorLabel->setText("Both participant names are required.");
            return;
        }
        if (!isValidParticipantName(participantNameFor(session, KingdomId::White))
            || !isValidParticipantName(participantNameFor(session, KingdomId::Black))) {
            m_createErrorLabel->setText("Names cannot contain quotes or line breaks.");
            return;
        }

        if (m_multiplayerCheckBox && m_multiplayerCheckBox->isChecked()) {
            int port = 0;
            if (!tryParsePort(m_multiplayerPortEdit ? m_multiplayerPortEdit->getText().toStdString() : std::string{}, port)
                || !isValidMultiplayerPort(port)) {
                m_createErrorLabel->setText("Enter a valid LAN port between 1 and 65535.");
                return;
            }

            const std::string password = trimCopy(
                m_multiplayerPasswordEdit ? m_multiplayerPasswordEdit->getText().toStdString() : std::string{});
            if (password.empty()) {
                m_createErrorLabel->setText("A multiplayer password is required.");
                return;
            }

            session.multiplayer.enabled = true;
            session.multiplayer.port = port;
            session.multiplayer.passwordSalt = MultiplayerPasswordUtils::generateSalt();
            session.multiplayer.passwordHash = MultiplayerPasswordUtils::computePasswordDigest(
                password, session.multiplayer.passwordSalt);
            session.multiplayer.protocolVersion = kCurrentMultiplayerProtocolVersion;
        }

        if (m_onCreateSave) {
            const std::string error = m_onCreateSave(session);
            if (!error.empty()) {
                m_createErrorLabel->setText(error);
                return;
            }
        }

        closeCreateDialog();
    });
    dialog->add(createButton);

    m_joinOverlay = tgui::Panel::create({"100%", "100%"});
    m_joinOverlay->getRenderer()->setBackgroundColor(tgui::Color(0, 0, 0, 170));
    m_joinOverlay->setVisible(false);
    m_panel->add(m_joinOverlay);

    auto joinDialog = tgui::Panel::create({500, 390});
    joinDialog->setPosition({"(&.parent.width - width) / 2", "(&.parent.height - height) / 2"});
    joinDialog->getRenderer()->setBackgroundColor(tgui::Color(46, 46, 46, 245));
    joinDialog->getRenderer()->setBorderColor(tgui::Color(120, 120, 120));
    joinDialog->getRenderer()->setBorders(2);
    m_joinOverlay->add(joinDialog);

    auto joinTitle = tgui::Label::create("Join Multiplayer");
    joinTitle->setPosition({"(&.width - width) / 2", 18});
    joinTitle->setTextSize(26);
    joinTitle->getRenderer()->setTextColor(tgui::Color::White);
    joinDialog->add(joinTitle);

    auto joinHostLabel = tgui::Label::create("Server IP");
    joinHostLabel->setPosition({36, 86});
    joinHostLabel->setTextSize(18);
    joinHostLabel->getRenderer()->setTextColor(tgui::Color::White);
    joinDialog->add(joinHostLabel);

    m_joinHostEdit = tgui::EditBox::create();
    m_joinHostEdit->setPosition({36, 114});
    m_joinHostEdit->setSize({428, 34});
    joinDialog->add(m_joinHostEdit);

    auto joinPortLabel = tgui::Label::create("Server port");
    joinPortLabel->setPosition({36, 170});
    joinPortLabel->setTextSize(18);
    joinPortLabel->getRenderer()->setTextColor(tgui::Color::White);
    joinDialog->add(joinPortLabel);

    m_joinPortEdit = tgui::EditBox::create();
    m_joinPortEdit->setPosition({36, 198});
    m_joinPortEdit->setSize({180, 34});
    m_joinPortEdit->setInputValidator(tgui::EditBox::Validator::UInt);
    joinDialog->add(m_joinPortEdit);

    auto joinPasswordLabel = tgui::Label::create("Password");
    joinPasswordLabel->setPosition({36, 254});
    joinPasswordLabel->setTextSize(18);
    joinPasswordLabel->getRenderer()->setTextColor(tgui::Color::White);
    joinDialog->add(joinPasswordLabel);

    m_joinPasswordEdit = tgui::EditBox::create();
    m_joinPasswordEdit->setPosition({36, 282});
    m_joinPasswordEdit->setSize({428, 34});
    m_joinPasswordEdit->setPasswordCharacter('*');
    joinDialog->add(m_joinPasswordEdit);

    m_joinErrorLabel = tgui::Label::create("");
    m_joinErrorLabel->setPosition({36, 332});
    m_joinErrorLabel->setSize({240, 40});
    m_joinErrorLabel->setAutoSize(false);
    m_joinErrorLabel->setTextSize(15);
    m_joinErrorLabel->getRenderer()->setTextColor(tgui::Color(255, 120, 120));
    joinDialog->add(m_joinErrorLabel);

    auto joinCancelButton = tgui::Button::create("Cancel");
    joinCancelButton->setPosition({292, 336});
    joinCancelButton->setSize({78, 30});
    joinCancelButton->onPress([this]() {
        closeJoinDialog();
    });
    joinDialog->add(joinCancelButton);

    auto joinButton = tgui::Button::create("Join Game");
    joinButton->setPosition({380, 336});
    joinButton->setSize({84, 30});
    joinButton->onPress([this]() {
        JoinMultiplayerRequest request;
        request.host = trimCopy(m_joinHostEdit ? m_joinHostEdit->getText().toStdString() : std::string{});
        request.password = trimCopy(m_joinPasswordEdit ? m_joinPasswordEdit->getText().toStdString() : std::string{});

        if (request.host.empty()) {
            m_joinErrorLabel->setText("Server IP is required.");
            return;
        }
        if (!tryParsePort(m_joinPortEdit ? m_joinPortEdit->getText().toStdString() : std::string{}, request.port)
            || !isValidMultiplayerPort(request.port)) {
            m_joinErrorLabel->setText("Enter a valid LAN port.");
            return;
        }
        if (request.password.empty()) {
            m_joinErrorLabel->setText("Password is required.");
            return;
        }

        if (m_onJoinMultiplayer) {
            const std::string error = m_onJoinMultiplayer(request);
            if (!error.empty()) {
                m_joinErrorLabel->setText(error);
                return;
            }
        }

        closeJoinDialog();
    });
    joinDialog->add(joinButton);

    updateCreateDialogLabels();
    updateSaveButtons();
    m_panel->setVisible(false);
}

void MainMenuUI::show() {
    if (m_panel) {
        m_panel->setVisible(true);
        showMainScreen();
    }
}

void MainMenuUI::hide() {
    if (m_panel) {
        closeCreateDialog();
        closeJoinDialog();
        m_panel->setVisible(false);
    }
}

void MainMenuUI::showLoadMenu() {
    if (!m_panel) {
        return;
    }

    m_panel->setVisible(true);
    if (m_mainBox) m_mainBox->setVisible(false);
    if (m_loadBox) m_loadBox->setVisible(true);
    closeCreateDialog();
    closeJoinDialog();
    updateSaveButtons();
}

void MainMenuUI::setSaves(const std::vector<SaveSummary>& saves) {
    m_saves = saves;
    if (!m_saveList) {
        return;
    }

    m_saveList->removeAllItems();
    for (const auto& save : m_saves) {
        m_saveList->addItem(buildSaveLabel(save), save.saveName);
    }

    if (m_emptyLabel) {
        m_emptyLabel->setVisible(m_saves.empty());
    }

    updateSaveButtons();
}

void MainMenuUI::setOnLoadSaves(std::function<void()> callback) { m_onLoadSaves = std::move(callback); }
void MainMenuUI::setOnExitGame(std::function<void()> callback) { m_onExitGame = std::move(callback); }
void MainMenuUI::setOnCreateSave(CreateSaveCallback callback) { m_onCreateSave = std::move(callback); }
void MainMenuUI::setOnPlaySave(std::function<void(const std::string&)> callback) { m_onPlaySave = std::move(callback); }
void MainMenuUI::setOnDeleteSave(std::function<void(const std::string&)> callback) { m_onDeleteSave = std::move(callback); }
void MainMenuUI::setOnJoinMultiplayer(JoinMultiplayerCallback callback) { m_onJoinMultiplayer = std::move(callback); }

void MainMenuUI::showMainScreen() {
    if (m_mainBox) m_mainBox->setVisible(true);
    if (m_loadBox) m_loadBox->setVisible(false);
    closeCreateDialog();
    closeJoinDialog();
}

void MainMenuUI::updateSaveButtons() {
    const bool hasSelection = !selectedSaveName().empty();
    if (m_deleteButton) m_deleteButton->setEnabled(hasSelection);
    if (m_playButton) m_playButton->setEnabled(hasSelection);
}

void MainMenuUI::updateCreateDialogLabels() {
    std::array<KingdomParticipantConfig, kNumKingdoms> participants =
        defaultKingdomParticipants(parseGameModeId(m_modeCombo ? m_modeCombo->getSelectedItemId() : tgui::String{}));

    if (m_whiteControllerCombo) {
        participants[kingdomIndex(KingdomId::White)].controller =
            parseControllerId(m_whiteControllerCombo->getSelectedItemId());
    }
    if (m_blackControllerCombo) {
        participants[kingdomIndex(KingdomId::Black)].controller =
            parseControllerId(m_blackControllerCombo->getSelectedItemId());
    }

    if (m_whiteRoleLabel) {
        m_whiteRoleLabel->setText(kingdomAssignmentLabel(participants, KingdomId::White));
    }
    if (m_blackRoleLabel) {
        m_blackRoleLabel->setText(kingdomAssignmentLabel(participants, KingdomId::Black));
    }
    if (m_whiteNameLabel) {
        m_whiteNameLabel->setText(participantNamePrompt(participants, KingdomId::White));
    }
    if (m_blackNameLabel) {
        m_blackNameLabel->setText(participantNamePrompt(participants, KingdomId::Black));
    }
    if (m_createErrorLabel) m_createErrorLabel->setText("");
    updateMultiplayerControlsVisibility();
}

void MainMenuUI::updateMultiplayerControlsVisibility() {
    std::array<KingdomParticipantConfig, kNumKingdoms> participants =
        defaultKingdomParticipants(parseGameModeId(m_modeCombo ? m_modeCombo->getSelectedItemId() : tgui::String{}));

    if (m_whiteControllerCombo) {
        participants[kingdomIndex(KingdomId::White)].controller =
            parseControllerId(m_whiteControllerCombo->getSelectedItemId());
    }
    if (m_blackControllerCombo) {
        participants[kingdomIndex(KingdomId::Black)].controller =
            parseControllerId(m_blackControllerCombo->getSelectedItemId());
    }

    const bool multiplayerAvailable = supportsMultiplayerParticipants(participants);
    if (m_multiplayerCheckBox && !multiplayerAvailable) {
        m_multiplayerCheckBox->setChecked(false);
    }

    const bool multiplayerEnabled = multiplayerAvailable
        && m_multiplayerCheckBox
        && m_multiplayerCheckBox->isChecked();

    if (m_multiplayerCheckBox) m_multiplayerCheckBox->setVisible(multiplayerAvailable);
    if (m_multiplayerHintLabel) m_multiplayerHintLabel->setVisible(multiplayerAvailable);
    if (m_multiplayerPortLabel) m_multiplayerPortLabel->setVisible(multiplayerEnabled);
    if (m_multiplayerPortEdit) m_multiplayerPortEdit->setVisible(multiplayerEnabled);
    if (m_multiplayerPasswordLabel) m_multiplayerPasswordLabel->setVisible(multiplayerEnabled);
    if (m_multiplayerPasswordEdit) m_multiplayerPasswordEdit->setVisible(multiplayerEnabled);
}

void MainMenuUI::applyPresetToCreateDialog(GameMode mode) {
    const auto defaults = defaultKingdomParticipants(mode);

    if (m_whiteControllerCombo) {
        m_whiteControllerCombo->setSelectedItemById(
            defaults[kingdomIndex(KingdomId::White)].controller == ControllerType::Human ? "human" : "ai");
    }
    if (m_blackControllerCombo) {
        m_blackControllerCombo->setSelectedItemById(
            defaults[kingdomIndex(KingdomId::Black)].controller == ControllerType::Human ? "human" : "ai");
    }
    if (m_whiteNameEdit) {
        m_whiteNameEdit->setText(defaults[kingdomIndex(KingdomId::White)].participantName);
    }
    if (m_blackNameEdit) {
        m_blackNameEdit->setText(defaults[kingdomIndex(KingdomId::Black)].participantName);
    }

    updateCreateDialogLabels();
}

void MainMenuUI::openCreateDialog() {
    if (!m_createOverlay) {
        return;
    }

    if (m_saveNameEdit) m_saveNameEdit->setText("");
    if (m_modeCombo) m_modeCombo->setSelectedItemById("human_ai");
    if (m_multiplayerCheckBox) m_multiplayerCheckBox->setChecked(false);
    if (m_multiplayerPortEdit) m_multiplayerPortEdit->setText("45000");
    if (m_multiplayerPasswordEdit) m_multiplayerPasswordEdit->setText("");
    applyPresetToCreateDialog(GameMode::HumanVsAI);
    closeJoinDialog();
    m_createOverlay->setVisible(true);
}

void MainMenuUI::closeCreateDialog() {
    if (m_createOverlay) m_createOverlay->setVisible(false);
    if (m_createErrorLabel) m_createErrorLabel->setText("");
}

void MainMenuUI::openJoinDialog() {
    if (!m_joinOverlay) {
        return;
    }

    if (m_joinHostEdit) m_joinHostEdit->setText("");
    if (m_joinPortEdit) m_joinPortEdit->setText("45000");
    if (m_joinPasswordEdit) m_joinPasswordEdit->setText("");
    if (m_joinErrorLabel) m_joinErrorLabel->setText("");
    closeCreateDialog();
    m_joinOverlay->setVisible(true);
}

void MainMenuUI::closeJoinDialog() {
    if (m_joinOverlay) m_joinOverlay->setVisible(false);
    if (m_joinErrorLabel) m_joinErrorLabel->setText("");
}

std::string MainMenuUI::selectedSaveName() const {
    if (!m_saveList) {
        return {};
    }

    return trimCopy(m_saveList->getSelectedItemId().toStdString());
}

std::string MainMenuUI::trimCopy(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

bool MainMenuUI::isValidSaveName(const std::string& value) {
    return !value.empty()
        && value.find_first_of("<>:\"/\\|?*") == std::string::npos
        && value.find('\n') == std::string::npos
        && value.find('\r') == std::string::npos;
}

bool MainMenuUI::isValidParticipantName(const std::string& value) {
    return !value.empty()
        && value.find('"') == std::string::npos
        && value.find('\\') == std::string::npos
        && value.find('\n') == std::string::npos
        && value.find('\r') == std::string::npos;
}

bool MainMenuUI::tryParsePort(const std::string& value, int& port) {
    const std::string trimmed = trimCopy(value);
    if (trimmed.empty()) {
        return false;
    }

    try {
        const long long parsed = std::stoll(trimmed);
        if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
            return false;
        }

        port = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

GameMode MainMenuUI::parseGameModeId(const tgui::String& id) {
    const std::string modeId = id.toStdString();
    if (modeId == "human_human") {
        return GameMode::HumanVsHuman;
    }
    if (modeId == "ai_ai") {
        return GameMode::AIvsAI;
    }
    return GameMode::HumanVsAI;
}

ControllerType MainMenuUI::parseControllerId(const tgui::String& id) {
    return id.toStdString() == "ai" ? ControllerType::AI : ControllerType::Human;
}
