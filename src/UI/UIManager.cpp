#include "UI/UIManager.hpp"
#include "Assets/AssetManager.hpp"
#include "Board/Cell.hpp"
#include "Units/Piece.hpp"
#include "Buildings/Building.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Config/GameConfig.hpp"
#include "UI/HUDLayout.hpp"

void UIManager::init(tgui::Gui& gui, const AssetManager& assets) {
    m_mainMenu.init(gui, assets);
    m_hud.init(gui, assets);
    m_pauseMenu.init(gui);

    m_leftSidebar = tgui::Panel::create(HUDLayout::sidebarSize(HUDAnchor::MiddleLeft));
    m_leftSidebar->setPosition(HUDLayout::sidebarPosition(HUDAnchor::MiddleLeft));
    HUDLayout::styleSidebarFrame(m_leftSidebar);
    gui.add(m_leftSidebar, "InGameLeftSidebar");

    m_leftEmptyState = tgui::Panel::create({"&.width - 24", "&.height - 24"});
    m_leftEmptyState->setPosition({12, 12});
    HUDLayout::styleEmbeddedPanel(m_leftEmptyState);
    m_leftSidebar->add(m_leftEmptyState);

    m_leftContextTitle = tgui::Label::create("Selection");
    m_leftContextTitle->setPosition({10, 10});
    HUDLayout::styleSidebarTitle(m_leftContextTitle);
    m_leftEmptyState->add(m_leftContextTitle);

    m_leftContextHint = tgui::Label::create("Select a piece or building.");
    m_leftContextHint->setPosition({10, 48});
    m_leftContextHint->setSize({316, 120});
    m_leftContextHint->setAutoSize(false);
    m_leftContextHint->setTextSize(14);
    m_leftContextHint->getRenderer()->setTextColor(tgui::Color(220, 220, 220));
    m_leftEmptyState->add(m_leftContextHint);

    m_rightSidebar = tgui::Panel::create(HUDLayout::sidebarSize(HUDAnchor::MiddleRight));
    m_rightSidebar->setPosition(HUDLayout::sidebarPosition(HUDAnchor::MiddleRight));
    HUDLayout::styleSidebarFrame(m_rightSidebar);
    gui.add(m_rightSidebar, "InGameRightSidebar");

    m_rightHistorySection = tgui::Panel::create({"&.width - 24", "(&.height - 36) / 2"});
    m_rightHistorySection->setPosition({12, 12});
    HUDLayout::styleSidebarSection(m_rightHistorySection);
    m_rightSidebar->add(m_rightHistorySection);

    m_rightBalanceSection = tgui::Panel::create({"&.width - 24", "(&.height - 36) / 2"});
    m_rightBalanceSection->setPosition({"12", "24 + ((&.height - 36) / 2)"});
    HUDLayout::styleSidebarSection(m_rightBalanceSection);
    m_rightSidebar->add(m_rightBalanceSection);

    m_piecePanel.init(m_leftSidebar);
    m_buildingPanel.init(m_leftSidebar);
    m_barracksPanel.init(m_leftSidebar);
    m_buildToolPanel.init(m_leftSidebar);
    m_cellPanel.init(m_leftSidebar);
    m_eventLogPanel.init(m_rightHistorySection);
    m_kingdomBalancePanel.init(m_rightBalanceSection);
    m_toolBar.init(gui);

    m_multiplayerWaitingOverlay = tgui::Panel::create({"100%", "100%"});
    m_multiplayerWaitingOverlay->getRenderer()->setBackgroundColor(tgui::Color(0, 0, 0, 140));
    gui.add(m_multiplayerWaitingOverlay, "MultiplayerWaitingOverlay");

    auto waitingDialog = tgui::Panel::create({540, 230});
    waitingDialog->setPosition({"(&.parent.width - width) / 2", "(&.parent.height - height) / 2"});
    waitingDialog->getRenderer()->setBackgroundColor(tgui::Color(32, 32, 32, 240));
    waitingDialog->getRenderer()->setBorders(2);
    waitingDialog->getRenderer()->setBorderColor(tgui::Color(196, 160, 76));
    m_multiplayerWaitingOverlay->add(waitingDialog);

    m_multiplayerWaitingTitle = tgui::Label::create("Waiting for Player");
    m_multiplayerWaitingTitle->setPosition({24, 22});
    m_multiplayerWaitingTitle->setSize({492, 30});
    m_multiplayerWaitingTitle->setAutoSize(false);
    m_multiplayerWaitingTitle->setTextSize(26);
    m_multiplayerWaitingTitle->getRenderer()->setTextColor(tgui::Color::White);
    waitingDialog->add(m_multiplayerWaitingTitle);

    m_multiplayerWaitingMessage = tgui::Label::create("");
    m_multiplayerWaitingMessage->setPosition({24, 72});
    m_multiplayerWaitingMessage->setSize({492, 120});
    m_multiplayerWaitingMessage->setAutoSize(false);
    m_multiplayerWaitingMessage->setTextSize(17);
    m_multiplayerWaitingMessage->getRenderer()->setTextColor(tgui::Color(230, 230, 230));
    waitingDialog->add(m_multiplayerWaitingMessage);

    m_multiplayerWaitingButton = tgui::Button::create("Return to Menu");
    m_multiplayerWaitingButton->setPosition({372, 186});
    m_multiplayerWaitingButton->setSize({144, 30});
    m_multiplayerWaitingButton->onPress([this]() {
        const auto onClose = m_onMultiplayerWaitingClose;
        hideMultiplayerWaitingOverlay();
        if (onClose) {
            onClose();
        }
    });
    waitingDialog->add(m_multiplayerWaitingButton);

    m_multiplayerAlertOverlay = tgui::Panel::create({"100%", "100%"});
    m_multiplayerAlertOverlay->getRenderer()->setBackgroundColor(tgui::Color(0, 0, 0, 175));
    gui.add(m_multiplayerAlertOverlay, "MultiplayerAlertOverlay");

    auto alertDialog = tgui::Panel::create({520, 260});
    alertDialog->setPosition({"(&.parent.width - width) / 2", "(&.parent.height - height) / 2"});
    alertDialog->getRenderer()->setBackgroundColor(tgui::Color(40, 40, 40, 245));
    alertDialog->getRenderer()->setBorders(2);
    alertDialog->getRenderer()->setBorderColor(tgui::Color(140, 140, 140));
    m_multiplayerAlertOverlay->add(alertDialog);

    m_multiplayerAlertTitle = tgui::Label::create("Network Alert");
    m_multiplayerAlertTitle->setPosition({24, 20});
    m_multiplayerAlertTitle->setSize({472, 30});
    m_multiplayerAlertTitle->setAutoSize(false);
    m_multiplayerAlertTitle->setTextSize(24);
    m_multiplayerAlertTitle->getRenderer()->setTextColor(tgui::Color::White);
    alertDialog->add(m_multiplayerAlertTitle);

    m_multiplayerAlertMessage = tgui::Label::create("");
    m_multiplayerAlertMessage->setPosition({24, 66});
    m_multiplayerAlertMessage->setSize({472, 126});
    m_multiplayerAlertMessage->setAutoSize(false);
    m_multiplayerAlertMessage->setTextSize(17);
    m_multiplayerAlertMessage->getRenderer()->setTextColor(tgui::Color(230, 230, 230));
    alertDialog->add(m_multiplayerAlertMessage);

    m_multiplayerAlertButton = tgui::Button::create("OK");
    m_multiplayerAlertButton->setPosition({372, 208});
    m_multiplayerAlertButton->setSize({124, 34});
    m_multiplayerAlertButton->onPress([this]() {
        const auto onClose = m_onMultiplayerAlertClose;
        hideMultiplayerAlert();
        if (onClose) {
            onClose();
        }
    });
    alertDialog->add(m_multiplayerAlertButton);

    m_leftSidebar->setVisible(false);
    m_rightSidebar->setVisible(false);
    m_multiplayerWaitingOverlay->setVisible(false);
    m_multiplayerAlertOverlay->setVisible(false);
}

void UIManager::showMainMenu() {
    hideAllPanels();
    m_mainMenu.show();
}

void UIManager::showHUD() {
    m_mainMenu.hide();
    m_hud.show();
    m_toolBar.show();
    if (m_leftSidebar) m_leftSidebar->setVisible(false);
    if (m_rightSidebar) m_rightSidebar->setVisible(true);
    m_eventLogPanel.show();
    m_kingdomBalancePanel.show();
}

void UIManager::showPauseMenu() {
    m_pauseMenu.show();
}

void UIManager::hidePauseMenu() {
    m_pauseMenu.hide();
}

void UIManager::updateDashboard(const InGameViewModel& model) {
    m_hud.update(model);
    m_eventLogPanel.update(model.eventRows);
    m_kingdomBalancePanel.update(model.balanceMetrics);
}

void UIManager::showPiecePanel(const Piece& piece, const GameConfig& config, bool allowUpgrade) {
    hideLeftContextPanels();
    if (m_leftSidebar) m_leftSidebar->setVisible(true);
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    m_piecePanel.show(piece, config, allowUpgrade);
}

void UIManager::showBuildingPanel(const Building& building) {
    hideLeftContextPanels();
    if (m_leftSidebar) m_leftSidebar->setVisible(true);
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    m_buildingPanel.show(building);
}

void UIManager::showBarracksPanel(const Building& barracks, const Kingdom& kingdom, const GameConfig& config,
                                  bool allowProduce) {
    hideLeftContextPanels();
    if (m_leftSidebar) m_leftSidebar->setVisible(true);
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    m_barracksPanel.show(barracks, kingdom, config, allowProduce);
}

void UIManager::showBuildToolPanel(const Kingdom& kingdom, const GameConfig& config, bool allowBuild) {
    hideLeftContextPanels();
    if (m_leftSidebar) m_leftSidebar->setVisible(true);
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    m_buildToolPanel.show(kingdom, config, allowBuild);
}

void UIManager::showCellPanel(const Cell& cell) {
    hideLeftContextPanels();
    if (m_leftSidebar) m_leftSidebar->setVisible(true);
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    m_cellPanel.show(cell);
}

void UIManager::showSelectionEmptyState() {
    hideLeftContextPanels();
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    if (m_leftSidebar) m_leftSidebar->setVisible(false);
}

void UIManager::hideAllPanels() {
    m_mainMenu.hide();
    m_hud.hide();
    m_pauseMenu.hide();
    hideMultiplayerWaitingOverlay();
    hideMultiplayerAlert();
    clearMultiplayerStatus();
    hideLeftContextPanels();
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    m_eventLogPanel.hide();
    m_kingdomBalancePanel.hide();
    m_toolBar.hide();
    if (m_leftSidebar) m_leftSidebar->setVisible(false);
    if (m_rightSidebar) m_rightSidebar->setVisible(false);
}

void UIManager::update() {
    // Placeholder for animation updates
}

void UIManager::setMultiplayerStatus(const std::string& text, MultiplayerStatusTone tone) {
    m_hud.setMultiplayerStatus(text, tone);
}

void UIManager::clearMultiplayerStatus() {
    m_hud.clearMultiplayerStatus();
}

void UIManager::showMultiplayerWaitingOverlay(const std::string& title,
                                              const std::string& message,
                                              const std::string& buttonLabel,
                                              std::function<void()> onClose) {
    m_onMultiplayerWaitingClose = std::move(onClose);
    if (m_multiplayerWaitingTitle) {
        m_multiplayerWaitingTitle->setText(title);
    }
    if (m_multiplayerWaitingMessage) {
        m_multiplayerWaitingMessage->setText(message);
    }
    if (m_multiplayerWaitingButton) {
        m_multiplayerWaitingButton->setText(buttonLabel.empty() ? "Return to Menu" : buttonLabel);
        m_multiplayerWaitingButton->setVisible(!buttonLabel.empty() || static_cast<bool>(m_onMultiplayerWaitingClose));
    }
    if (m_multiplayerWaitingOverlay) {
        m_multiplayerWaitingOverlay->setVisible(true);
    }
}

void UIManager::hideMultiplayerWaitingOverlay() {
    if (m_multiplayerWaitingOverlay) {
        m_multiplayerWaitingOverlay->setVisible(false);
    }
    m_onMultiplayerWaitingClose = {};
}

bool UIManager::isMultiplayerWaitingOverlayVisible() const {
    return m_multiplayerWaitingOverlay && m_multiplayerWaitingOverlay->isVisible();
}

void UIManager::showMultiplayerAlert(const std::string& title,
                                     const std::string& message,
                                     const std::string& buttonLabel,
                                     std::function<void()> onClose) {
    m_onMultiplayerAlertClose = std::move(onClose);
    if (m_multiplayerAlertTitle) {
        m_multiplayerAlertTitle->setText(title);
    }
    if (m_multiplayerAlertMessage) {
        m_multiplayerAlertMessage->setText(message);
    }
    if (m_multiplayerAlertButton) {
        m_multiplayerAlertButton->setText(buttonLabel);
    }
    if (m_multiplayerAlertOverlay) {
        m_multiplayerAlertOverlay->setVisible(true);
    }
}

void UIManager::hideMultiplayerAlert() {
    if (m_multiplayerAlertOverlay) {
        m_multiplayerAlertOverlay->setVisible(false);
    }
    m_onMultiplayerAlertClose = {};
}

bool UIManager::isMultiplayerAlertVisible() const {
    return m_multiplayerAlertOverlay && m_multiplayerAlertOverlay->isVisible();
}

void UIManager::hideLeftContextPanels() {
    m_piecePanel.hide();
    m_buildingPanel.hide();
    m_barracksPanel.hide();
    m_buildToolPanel.hide();
    m_cellPanel.hide();
}

void UIManager::setLeftContextMessage(const std::string& title, const std::string& message) {
    if (!m_leftEmptyState) {
        return;
    }

    m_leftEmptyState->setVisible(true);
    if (m_leftContextTitle) {
        m_leftContextTitle->setText(title);
    }
    if (m_leftContextHint) {
        m_leftContextHint->setText(message);
    }
}
