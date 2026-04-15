#include "UI/MainMenuUI.hpp"

#include "Assets/AssetManager.hpp"

#include <cctype>

namespace {
std::string buildSaveLabel(const SaveSummary& save) {
    return save.saveName + " - " + gameModeLabel(save.mode);
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
    btnLoad->setPosition({"(&.width - width) / 2", "42%"});
    btnLoad->setSize({220, 52});
    btnLoad->onPress([this]() {
        if (m_onLoadSaves) m_onLoadSaves();
        showLoadMenu();
    });
    m_mainBox->add(btnLoad);

    auto btnExit = tgui::Button::create("Exit Game");
    btnExit->setPosition({"(&.width - width) / 2", "55%"});
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

    auto dialog = tgui::Panel::create({520, 410});
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
    m_saveNameEdit->setSize({448, 34});
    dialog->add(m_saveNameEdit);

    auto modeLabel = tgui::Label::create("Game mode");
    modeLabel->setPosition({36, 154});
    modeLabel->setTextSize(18);
    modeLabel->getRenderer()->setTextColor(tgui::Color::White);
    dialog->add(modeLabel);

    m_modeCombo = tgui::ComboBox::create();
    m_modeCombo->setPosition({36, 182});
    m_modeCombo->setSize({448, 34});
    m_modeCombo->addItem("Human vs AI", "human_ai");
    m_modeCombo->addItem("Human vs Human", "human_human");
    m_modeCombo->addItem("AI vs AI", "ai_ai");
    m_modeCombo->setSelectedItemById("human_ai");
    m_modeCombo->onItemSelect([this]() {
        updateCreateDialogLabels();
    });
    dialog->add(m_modeCombo);

    m_nameOneLabel = tgui::Label::create("Human player name");
    m_nameOneLabel->setPosition({36, 230});
    m_nameOneLabel->setTextSize(18);
    m_nameOneLabel->getRenderer()->setTextColor(tgui::Color::White);
    dialog->add(m_nameOneLabel);

    m_nameOneEdit = tgui::EditBox::create();
    m_nameOneEdit->setPosition({36, 258});
    m_nameOneEdit->setSize({448, 34});
    dialog->add(m_nameOneEdit);

    m_nameTwoLabel = tgui::Label::create("AI name");
    m_nameTwoLabel->setPosition({36, 304});
    m_nameTwoLabel->setTextSize(18);
    m_nameTwoLabel->getRenderer()->setTextColor(tgui::Color::White);
    dialog->add(m_nameTwoLabel);

    m_nameTwoEdit = tgui::EditBox::create();
    m_nameTwoEdit->setPosition({36, 332});
    m_nameTwoEdit->setSize({448, 34});
    dialog->add(m_nameTwoEdit);

    m_createErrorLabel = tgui::Label::create("");
    m_createErrorLabel->setPosition({36, 372});
    m_createErrorLabel->setTextSize(15);
    m_createErrorLabel->getRenderer()->setTextColor(tgui::Color(255, 120, 120));
    dialog->add(m_createErrorLabel);

    auto cancelButton = tgui::Button::create("Cancel");
    cancelButton->setPosition({292, 372});
    cancelButton->setSize({92, 30});
    cancelButton->onPress([this]() {
        closeCreateDialog();
    });
    dialog->add(cancelButton);

    auto createButton = tgui::Button::create("Create");
    createButton->setPosition({392, 372});
    createButton->setSize({92, 30});
    createButton->onPress([this]() {
        GameSessionConfig session;
        session.saveName = trimCopy(m_saveNameEdit->getText().toStdString());
        session.mode = parseGameModeId(m_modeCombo->getSelectedItemId());
        session.participantNames[0] = trimCopy(m_nameOneEdit->getText().toStdString());
        session.participantNames[1] = trimCopy(m_nameTwoEdit->getText().toStdString());

        if (session.saveName.empty()) {
            m_createErrorLabel->setText("Save name is required.");
            return;
        }
        if (!isValidSaveName(session.saveName)) {
            m_createErrorLabel->setText("Save name contains invalid characters.");
            return;
        }
        if (session.participantNames[0].empty() || session.participantNames[1].empty()) {
            m_createErrorLabel->setText("Both participant names are required.");
            return;
        }
        if (!isValidParticipantName(session.participantNames[0])
            || !isValidParticipantName(session.participantNames[1])) {
            m_createErrorLabel->setText("Names cannot contain quotes or line breaks.");
            return;
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

void MainMenuUI::showMainScreen() {
    if (m_mainBox) m_mainBox->setVisible(true);
    if (m_loadBox) m_loadBox->setVisible(false);
    closeCreateDialog();
}

void MainMenuUI::updateSaveButtons() {
    const bool hasSelection = !selectedSaveName().empty();
    if (m_deleteButton) m_deleteButton->setEnabled(hasSelection);
    if (m_playButton) m_playButton->setEnabled(hasSelection);
}

void MainMenuUI::updateCreateDialogLabels() {
    const GameMode mode = parseGameModeId(m_modeCombo ? m_modeCombo->getSelectedItemId() : tgui::String{});
    const auto defaults = defaultParticipantNames(mode);

    if (m_nameOneLabel) m_nameOneLabel->setText(participantPrompt(mode, KingdomId::White));
    if (m_nameTwoLabel) m_nameTwoLabel->setText(participantPrompt(mode, KingdomId::Black));
    if (m_nameOneEdit) m_nameOneEdit->setText(defaults[0]);
    if (m_nameTwoEdit) m_nameTwoEdit->setText(defaults[1]);
    if (m_createErrorLabel) m_createErrorLabel->setText("");
}

void MainMenuUI::openCreateDialog() {
    if (!m_createOverlay) {
        return;
    }

    if (m_saveNameEdit) m_saveNameEdit->setText("");
    if (m_modeCombo) m_modeCombo->setSelectedItemById("human_ai");
    updateCreateDialogLabels();
    m_createOverlay->setVisible(true);
}

void MainMenuUI::closeCreateDialog() {
    if (m_createOverlay) m_createOverlay->setVisible(false);
    if (m_createErrorLabel) m_createErrorLabel->setText("");
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
