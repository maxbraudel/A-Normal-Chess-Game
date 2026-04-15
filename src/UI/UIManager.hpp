#pragma once
#include "UI/MainMenuUI.hpp"
#include "UI/HUD.hpp"
#include "UI/PauseMenuUI.hpp"
#include "UI/PiecePanel.hpp"
#include "UI/BuildingPanel.hpp"
#include "UI/BarracksPanel.hpp"
#include "UI/BuildToolPanel.hpp"
#include "UI/CellPanel.hpp"
#include "UI/EventLogPanel.hpp"
#include "UI/KingdomBalancePanel.hpp"
#include "UI/InGameViewModel.hpp"
#include "UI/ToolBar.hpp"

class AssetManager;
class Piece;
class Building;
class Kingdom;
class GameConfig;
class EventLog;
struct Cell;

class UIManager {
public:
    void init(tgui::Gui& gui, const AssetManager& assets);
    void showMainMenu();
    void showHUD();
    void showPauseMenu();
    void hidePauseMenu();
    void updateDashboard(const InGameViewModel& model);
    void showPiecePanel(const Piece& piece, const GameConfig& config, bool allowUpgrade);
    void showBuildingPanel(const Building& building);
    void showBarracksPanel(const Building& barracks, const Kingdom& kingdom, const GameConfig& config,
                           bool allowProduce);
    void showBuildToolPanel(const Kingdom& kingdom, const GameConfig& config, bool allowBuild);
    void showCellPanel(const Cell& cell);
    void showSelectionEmptyState();
    void showJournalContext();
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
    CellPanel&      cellPanel()       { return m_cellPanel; }
    EventLogPanel&  eventLogPanel()   { return m_eventLogPanel; }
    ToolBar&        toolBar()         { return m_toolBar; }

private:
    void hideLeftContextPanels();
    void setLeftContextMessage(const std::string& title, const std::string& message);

    MainMenuUI      m_mainMenu;
    HUD             m_hud;
    PauseMenuUI     m_pauseMenu;
    PiecePanel      m_piecePanel;
    BuildingPanel   m_buildingPanel;
    BarracksPanel   m_barracksPanel;
    BuildToolPanel  m_buildToolPanel;
    CellPanel       m_cellPanel;
    EventLogPanel   m_eventLogPanel;
    KingdomBalancePanel m_kingdomBalancePanel;
    ToolBar         m_toolBar;
    tgui::Panel::Ptr m_leftSidebar;
    tgui::Panel::Ptr m_leftEmptyState;
    tgui::Label::Ptr m_leftContextTitle;
    tgui::Label::Ptr m_leftContextHint;
    tgui::Panel::Ptr m_rightSidebar;
    tgui::Panel::Ptr m_rightHistorySection;
    tgui::Panel::Ptr m_rightBalanceSection;
};
