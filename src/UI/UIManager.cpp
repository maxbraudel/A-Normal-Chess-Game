#include "UI/UIManager.hpp"
#include "Assets/AssetManager.hpp"
#include "Board/Cell.hpp"
#include "Units/Piece.hpp"
#include "Buildings/Building.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/EventLog.hpp"
#include "UI/HUDLayout.hpp"

void UIManager::init(tgui::Gui& gui, const AssetManager& assets) {
    m_mainMenu.init(gui, assets);
    m_hud.init(gui, assets);
    m_pauseMenu.init(gui);

    m_leftSidebar = tgui::Panel::create(HUDLayout::sidebarSize());
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

    m_leftContextHint = tgui::Label::create("Select a piece, building, or cell.");
    m_leftContextHint->setPosition({10, 48});
    m_leftContextHint->setSize({316, 120});
    m_leftContextHint->setAutoSize(false);
    m_leftContextHint->setTextSize(14);
    m_leftContextHint->getRenderer()->setTextColor(tgui::Color(220, 220, 220));
    m_leftEmptyState->add(m_leftContextHint);

    m_rightSidebar = tgui::Panel::create(HUDLayout::sidebarSize());
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

    m_leftSidebar->setVisible(false);
    m_rightSidebar->setVisible(false);
}

void UIManager::showMainMenu() {
    hideAllPanels();
    m_mainMenu.show();
}

void UIManager::showHUD() {
    m_mainMenu.hide();
    m_hud.show();
    m_toolBar.show();
    if (m_leftSidebar) m_leftSidebar->setVisible(true);
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
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    m_piecePanel.show(piece, config, allowUpgrade);
}

void UIManager::showBuildingPanel(const Building& building) {
    hideLeftContextPanels();
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    m_buildingPanel.show(building);
}

void UIManager::showBarracksPanel(const Building& barracks, const Kingdom& kingdom, const GameConfig& config,
                                  bool allowProduce) {
    hideLeftContextPanels();
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    m_barracksPanel.show(barracks, kingdom, config, allowProduce);
}

void UIManager::showBuildToolPanel(const Kingdom& kingdom, const GameConfig& config, bool allowBuild) {
    hideLeftContextPanels();
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    m_buildToolPanel.show(kingdom, config, allowBuild);
}

void UIManager::showCellPanel(const Cell& cell) {
    hideLeftContextPanels();
    if (m_leftEmptyState) m_leftEmptyState->setVisible(false);
    m_cellPanel.show(cell);
}

void UIManager::showSelectionEmptyState() {
    hideLeftContextPanels();
    setLeftContextMessage("Selection", "Select a piece, building, or cell.");
}

void UIManager::showJournalContext() {
    hideLeftContextPanels();
    setLeftContextMessage("Journal", "History and kingdom balance stay pinned in the right panel.");
}

void UIManager::showEventLogPanel(const EventLog& log) {
    (void) log;
    showJournalContext();
}

void UIManager::hideAllPanels() {
    m_mainMenu.hide();
    m_hud.hide();
    m_pauseMenu.hide();
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
