#pragma once
#include "UI/MainMenuUI.hpp"
#include "UI/HUD.hpp"
#include "UI/PauseMenuUI.hpp"
#include "UI/PiecePanel.hpp"
#include "UI/BuildingPanel.hpp"
#include "UI/BarracksPanel.hpp"
#include "UI/BuildToolPanel.hpp"
#include "UI/EventLogPanel.hpp"
#include "UI/ToolBar.hpp"

class AssetManager;
class Piece;
class Building;
class Kingdom;
class GameConfig;
class EventLog;

class UIManager {
public:
    void init(tgui::Gui& gui, const AssetManager& assets);
    void showMainMenu();
    void showHUD();
    void showPauseMenu();
    void hidePauseMenu();
    void showPiecePanel(const Piece& piece, const GameConfig& config, bool allowUpgrade);
    void showBuildingPanel(const Building& building);
    void showBarracksPanel(const Building& barracks, const Kingdom& kingdom, const GameConfig& config,
                           bool allowProduce);
    void showBuildToolPanel(const Kingdom& kingdom, const GameConfig& config);
    void showEventLogPanel(const EventLog& log);
    void hideAllPanels();
    void update();

    MainMenuUI&     mainMenu()        { return m_mainMenu; }
    HUD&            hud()             { return m_hud; }
    PauseMenuUI&    pauseMenu()       { return m_pauseMenu; }
    PiecePanel&     piecePanel()      { return m_piecePanel; }
    BuildingPanel&  buildingPanel()   { return m_buildingPanel; }
    BarracksPanel&  barracksPanel()   { return m_barracksPanel; }
    BuildToolPanel& buildToolPanel()  { return m_buildToolPanel; }
    EventLogPanel&  eventLogPanel()   { return m_eventLogPanel; }
    ToolBar&        toolBar()         { return m_toolBar; }

private:
    MainMenuUI      m_mainMenu;
    HUD             m_hud;
    PauseMenuUI     m_pauseMenu;
    PiecePanel      m_piecePanel;
    BuildingPanel   m_buildingPanel;
    BarracksPanel   m_barracksPanel;
    BuildToolPanel  m_buildToolPanel;
    EventLogPanel   m_eventLogPanel;
    ToolBar         m_toolBar;
};
