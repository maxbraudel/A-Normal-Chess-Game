#pragma once
#include "UI/MainMenuUI.hpp"
#include "UI/HUD.hpp"
#include "UI/GameMenuUI.hpp"
#include "UI/PiecePanel.hpp"
#include "UI/BuildingPanel.hpp"
#include "UI/BarracksPanel.hpp"
#include "UI/BuildToolPanel.hpp"
#include "UI/CellPanel.hpp"
#include "UI/EventLogPanel.hpp"
#include "UI/KingdomBalancePanel.hpp"
#include "UI/InGameViewModel.hpp"
#include "UI/ToolBar.hpp"

#include <SFML/System/Vector2.hpp>
#include <functional>
#include <optional>
#include <string>

class AssetManager;
class Piece;
class Building;
class Kingdom;
class GameConfig;
struct Cell;

struct MultiplayerDialogAction {
    std::string label;
    std::function<void()> callback;
};

class UIManager {
public:
    void init(tgui::Gui& gui, const AssetManager& assets);
    void showMainMenu();
    void showHUD();
    void showGameMenu(const GameMenuPresentation& presentation);
    void hideGameMenu();
    bool isGameMenuVisible() const;
    void updateDashboard(const InGameViewModel& model);
    void showPiecePanel(const Piece& piece, const GameConfig& config, bool allowUpgrade);
    void showBuildingPanel(const Building& building);
    void showBarracksPanel(const Building& barracks, const Kingdom& kingdom, const GameConfig& config,
                           bool allowProduce);
    void showBuildToolPanel(const Kingdom& kingdom, const GameConfig& config, bool allowBuild);
    void showCellPanel(const Cell& cell);
    void showSelectionEmptyState();
    void hideAllPanels();
    void update();
    void setMultiplayerStatus(const std::string& text, MultiplayerStatusTone tone);
    void clearMultiplayerStatus();
    void showMultiplayerWaitingOverlay(const std::string& title,
                                       const std::string& message,
                                       const std::string& buttonLabel = "",
                                       std::function<void()> onClose = {});
    void hideMultiplayerWaitingOverlay();
    bool isMultiplayerWaitingOverlayVisible() const;
    void showMultiplayerAlert(const std::string& title,
                              const std::string& message,
                              const std::string& buttonLabel = "OK",
                              std::function<void()> onClose = {});
    void showMultiplayerAlert(const std::string& title,
                              const std::string& message,
                              const MultiplayerDialogAction& primaryAction,
                              std::optional<MultiplayerDialogAction> secondaryAction = std::nullopt);
    void hideMultiplayerAlert();
    bool isMultiplayerAlertVisible() const;
    bool blocksWorldMouseInput(const sf::Vector2i& screenPos, const sf::Vector2u& windowSize) const;

    MainMenuUI&     mainMenu()        { return m_mainMenu; }
    HUD&            hud()             { return m_hud; }
    GameMenuUI&     gameMenu()        { return m_gameMenu; }
    PiecePanel&     piecePanel()      { return m_piecePanel; }
    BuildingPanel&  buildingPanel()   { return m_buildingPanel; }
    BarracksPanel&  barracksPanel()   { return m_barracksPanel; }
    BuildToolPanel& buildToolPanel()  { return m_buildToolPanel; }
    CellPanel&      cellPanel()       { return m_cellPanel; }
    EventLogPanel&  eventLogPanel()   { return m_eventLogPanel; }
    ToolBar&        toolBar()         { return m_toolBar; }

private:
    enum class LeftContextView {
        None,
        Piece,
        Building,
        Barracks,
        BuildTool,
        Cell
    };

    void activateLeftContext(LeftContextView view);
    void hideLeftContextPanels();
    void setLeftContextMessage(const std::string& title, const std::string& message);

    MainMenuUI      m_mainMenu;
    HUD             m_hud;
    GameMenuUI      m_gameMenu;
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
    tgui::Panel::Ptr m_multiplayerWaitingOverlay;
    tgui::Label::Ptr m_multiplayerWaitingTitle;
    tgui::Label::Ptr m_multiplayerWaitingMessage;
    tgui::Button::Ptr m_multiplayerWaitingButton;
    tgui::Panel::Ptr m_multiplayerAlertOverlay;
    tgui::Label::Ptr m_multiplayerAlertTitle;
    tgui::Label::Ptr m_multiplayerAlertMessage;
    tgui::Button::Ptr m_multiplayerAlertPrimaryButton;
    tgui::Button::Ptr m_multiplayerAlertSecondaryButton;
    std::function<void()> m_onMultiplayerWaitingClose;
    std::function<void()> m_onMultiplayerAlertPrimaryAction;
    std::function<void()> m_onMultiplayerAlertSecondaryAction;
    LeftContextView m_currentLeftContext = LeftContextView::None;
};
