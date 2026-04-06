#include "UI/UIManager.hpp"
#include "Assets/AssetManager.hpp"
#include "Units/Piece.hpp"
#include "Buildings/Building.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/EventLog.hpp"

void UIManager::init(tgui::Gui& gui, const AssetManager& assets) {
    m_mainMenu.init(gui, assets);
    m_hud.init(gui, assets);
    m_pauseMenu.init(gui);
    m_piecePanel.init(gui);
    m_buildingPanel.init(gui);
    m_barracksPanel.init(gui);
    m_buildToolPanel.init(gui);
    m_eventLogPanel.init(gui);
    m_toolBar.init(gui);
}

void UIManager::showMainMenu() {
    hideAllPanels();
    m_mainMenu.show();
}

void UIManager::showHUD() {
    m_mainMenu.hide();
    m_hud.show();
    m_toolBar.show();
}

void UIManager::showPauseMenu() {
    m_pauseMenu.show();
}

void UIManager::hidePauseMenu() {
    m_pauseMenu.hide();
}

void UIManager::showPiecePanel(const Piece& piece, const GameConfig& config) {
    m_buildingPanel.hide();
    m_barracksPanel.hide();
    m_buildToolPanel.hide();
    m_eventLogPanel.hide();
    m_piecePanel.show(piece, config);
}

void UIManager::showBuildingPanel(const Building& building) {
    m_piecePanel.hide();
    m_barracksPanel.hide();
    m_buildToolPanel.hide();
    m_eventLogPanel.hide();
    m_buildingPanel.show(building);
}

void UIManager::showBarracksPanel(const Building& barracks, const Kingdom& kingdom, const GameConfig& config) {
    m_piecePanel.hide();
    m_buildingPanel.hide();
    m_buildToolPanel.hide();
    m_eventLogPanel.hide();
    m_barracksPanel.show(barracks, kingdom, config);
}

void UIManager::showBuildToolPanel(const Kingdom& kingdom, const GameConfig& config) {
    m_piecePanel.hide();
    m_buildingPanel.hide();
    m_barracksPanel.hide();
    m_eventLogPanel.hide();
    m_buildToolPanel.show(kingdom, config);
}

void UIManager::showEventLogPanel(const EventLog& log) {
    m_piecePanel.hide();
    m_buildingPanel.hide();
    m_barracksPanel.hide();
    m_buildToolPanel.hide();
    m_eventLogPanel.show(log);
}

void UIManager::hideAllPanels() {
    m_mainMenu.hide();
    m_hud.hide();
    m_pauseMenu.hide();
    m_piecePanel.hide();
    m_buildingPanel.hide();
    m_barracksPanel.hide();
    m_buildToolPanel.hide();
    m_eventLogPanel.hide();
    m_toolBar.hide();
}

void UIManager::update() {
    // Placeholder for animation updates
}
