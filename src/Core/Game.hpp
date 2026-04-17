#pragma once
#include "Core/LiveResizeRenderWindow.hpp"
#include "Core/AITurnRunner.hpp"
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <array>
#include <vector>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "Core/GameState.hpp"
#include "Core/InteractionPermissions.hpp"
#include "Core/TurnPhase.hpp"
#include "Core/GameClock.hpp"
#include "Core/GameEngine.hpp"
#include "Core/TurnDraft.hpp"
#include "Config/GameConfig.hpp"
#include "Debug/GameStateDebugRecorder.hpp"
#include "Systems/CheckSystem.hpp"
#include "AI/AIDirector.hpp"
#include "Input/InputHandler.hpp"
#include "Input/InputSelectionBookmark.hpp"
#include "Render/Camera.hpp"
#include "Render/Renderer.hpp"
#include "Runtime/AITurnCoordinator.hpp"
#include "Runtime/BuildOverlayCoordinator.hpp"
#include "Runtime/FrontendCoordinator.hpp"
#include "Runtime/InGamePresentationCoordinator.hpp"
#include "Runtime/MultiplayerJoinCoordinator.hpp"
#include "Runtime/MultiplayerRuntimeCoordinator.hpp"
#include "Runtime/PanelActionCoordinator.hpp"
#include "Runtime/PendingTurnValidationCache.hpp"
#include "Runtime/SelectionQueryCoordinator.hpp"
#include "Runtime/SessionFlow.hpp"
#include "Runtime/SessionPresentationCoordinator.hpp"
#include "Runtime/SessionRuntimeCoordinator.hpp"
#include "Runtime/TurnCoordinator.hpp"
#include "Runtime/TurnDraftCoordinator.hpp"
#include "Runtime/TurnLifecycleCoordinator.hpp"
#include "Runtime/UICallbackCoordinator.hpp"
#include "Assets/AssetManager.hpp"
#include "UI/UIManager.hpp"
#include "Core/LocalPlayerContext.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Multiplayer/MultiplayerRuntime.hpp"
#include "Save/SaveManager.hpp"
#include "Systems/CheckResponseRules.hpp"
#include "Systems/WeatherTypes.hpp"

#include <cstdint>

#ifdef _WIN32
LRESULT CALLBACK GameWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
#endif

class Game {
public:
    Game();
    void run();

private:
    void init();
    void handleInput();
    void update();
    void render();
    void handleWindowResize(sf::Vector2u newSize);

    bool startNewGame(const GameSessionConfig& session, std::string* errorMessage = nullptr);
    bool loadGame(const std::string& saveName);
    bool saveGame();
    void commitPlayerTurn();
    void resetPlayerTurn();
    void discardPendingAITurn();
    void refreshTurnPhase();
    void stopMultiplayer();
    bool joinMultiplayer(const JoinMultiplayerRequest& request, std::string* errorMessage = nullptr);
    bool reconnectToMultiplayerHost(std::string* errorMessage = nullptr);
    void updateMultiplayer();
    void commitAuthoritativeTurn();
    bool pushSnapshotToRemote(std::string* errorMessage = nullptr);
    void centerCameraOnKingdom(KingdomId kingdom);
    void configureLocalPlayerContext(const GameSessionConfig& session);
    FrontendRuntimeState makeFrontendRuntimeState() const;
    bool isLocalPlayerTurn() const;
    bool canLocalPlayerIssueCommands() const;
    KingdomId localPerspectiveKingdom() const;
    InGameHudPresentation buildInGameHudPresentation() const;
    bool isMultiplayerSessionReady() const;
    void returnToMainMenu();
    void openInGameMenu();
    void closeInGameMenu();
    void toggleInGameMenu();
    bool isInGameMenuOpen() const;
    bool isLanHost() const { return m_localPlayerContext.mode == LocalSessionMode::LanHost; }
    bool isLanClient() const { return m_localPlayerContext.mode == LocalSessionMode::LanClient; }
    std::string participantName(KingdomId id) const;
    std::string activeTurnLabel() const;

    void setupUICallbacks();
    void updateUIState();
    UICallbackRuntimeState makeUICallbackRuntimeState() const;
    UICallbackCoordinatorDependencies makeUICallbackCoordinatorDependencies();
    SessionRuntimeCallbacks makeSessionRuntimeCallbacks();
    TurnLifecycleCallbacks makeTurnLifecycleCallbacks();
    TurnDraftRuntimeState makeTurnDraftRuntimeState() const;
    BuildOverlayRuntimeState makeBuildOverlayRuntimeState(const InteractionPermissions& permissions) const;
    PendingTurnValidationCacheKey makePendingTurnValidationCacheKey() const;
    const CheckTurnValidation& validateActivePendingTurn() const;
    void invalidatePendingTurnValidation();
    bool canQueueNonMoveActions() const;
    InteractionPermissions currentInteractionPermissions(const CheckTurnValidation* validation = nullptr) const;
    TurnValidationContext authoritativeTurnContext() const;
    InputContext buildInputContext(const InteractionPermissions& permissions);
    bool shouldUseTurnDraft() const;
    void invalidateTurnDraft();
    void ensureTurnDraftUpToDate();
    SelectionQueryView makeSelectionQueryView();
    InputSelectionBookmark captureSelectionBookmark() const;
    void reconcileSelectionBookmark(const InputSelectionBookmark& bookmark);
    Piece* selectedDisplayedPiece();
    Building* selectedDisplayedBuilding();
    MapObject* selectedDisplayedMapObject();
    void activateSelectTool();
    void refreshBuildableCellsOverlay(const InteractionPermissions& permissions);
    void ensureWeatherMaskUpToDate();
    void triggerCheatcodeWeatherFront();
    void triggerCheatcodeChestSpawn();
    void triggerCheatcodeInfernalSpawn();
    Board& displayedBoard();
    const Board& displayedBoard() const;
    std::array<Kingdom, kNumKingdoms>& displayedKingdoms();
    const std::array<Kingdom, kNumKingdoms>& displayedKingdoms() const;
    std::vector<Building>& displayedPublicBuildings();
    const std::vector<Building>& displayedPublicBuildings() const;
    std::vector<MapObject>& displayedMapObjects();
    const std::vector<MapObject>& displayedMapObjects() const;
    std::vector<AutonomousUnit>& displayedAutonomousUnits();
    const std::vector<AutonomousUnit>& displayedAutonomousUnits() const;
    Kingdom& displayedKingdom(KingdomId id);
    const Kingdom& displayedKingdom(KingdomId id) const;

#ifdef _WIN32
    friend LRESULT CALLBACK GameWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void installWindowProcHook();
    void handleNativeResize(sf::Vector2u newSize);
#endif

    Board& board() { return m_engine.board(); }
    const Board& board() const { return m_engine.board(); }

    std::array<Kingdom, kNumKingdoms>& kingdoms() { return m_engine.kingdoms(); }
    const std::array<Kingdom, kNumKingdoms>& kingdoms() const { return m_engine.kingdoms(); }

    std::vector<Building>& publicBuildings() { return m_engine.publicBuildings(); }
    const std::vector<Building>& publicBuildings() const { return m_engine.publicBuildings(); }

    std::vector<MapObject>& mapObjects() { return m_engine.mapObjects(); }
    const std::vector<MapObject>& mapObjects() const { return m_engine.mapObjects(); }

    std::vector<AutonomousUnit>& autonomousUnits() { return m_engine.autonomousUnits(); }
    const std::vector<AutonomousUnit>& autonomousUnits() const { return m_engine.autonomousUnits(); }

    TurnSystem& turnSystem() { return m_engine.turnSystem(); }
    const TurnSystem& turnSystem() const { return m_engine.turnSystem(); }

    EventLog& eventLog() { return m_engine.eventLog(); }
    const EventLog& eventLog() const { return m_engine.eventLog(); }

    PieceFactory& pieceFactory() { return m_engine.pieceFactory(); }
    BuildingFactory& buildingFactory() { return m_engine.buildingFactory(); }

    std::string gameName() const { return m_engine.gameName(); }

    // ---- Kingdom helpers (generic, side-agnostic) ----
    Kingdom&       kingdom(KingdomId id)       { return m_engine.kingdom(id); }
    const Kingdom& kingdom(KingdomId id) const { return m_engine.kingdom(id); }

    Kingdom&       activeKingdom()       { return m_engine.activeKingdom(); }
    const Kingdom& activeKingdom() const { return m_engine.activeKingdom(); }

    Kingdom&       enemyKingdom()       { return m_engine.enemyKingdom(); }
    const Kingdom& enemyKingdom() const { return m_engine.enemyKingdom(); }

    // SFML / TGUI
    LiveResizeRenderWindow m_window;
    tgui::Gui m_gui;
    sf::View m_hudView;
    sf::Vector2u m_windowSize{1280u, 720u};

    // State
    GameState m_state;
    TurnPhase m_turnPhase = TurnPhase::WhiteTurn;
    GameClock m_clock;

    // Config
    GameConfig m_config;

    GameEngine m_engine;
    TurnDraft m_turnDraft;
    std::uint64_t m_lastTurnDraftRevision = 0;
    GameStateDebugRecorder m_debugRecorder;

    // AI
    AIDirector m_aiDirector;
    AITurnRunner m_aiTurnRunner;
    AITurnCoordinator m_aiTurnCoordinator;

    // Input/Render/UI
    InputHandler m_input;
    Camera m_camera;
    Renderer m_renderer;
    AssetManager m_assets;
    UIManager m_uiManager;
    SaveManager m_saveManager;
    LocalPlayerContext m_localPlayerContext;
    MultiplayerRuntime m_multiplayer;
    SessionFlow m_sessionFlow;
    MultiplayerJoinCoordinator m_multiplayerJoinCoordinator;
    TurnCoordinator m_turnCoordinator;
    bool m_waitingForRemoteTurnResult = false;
    SessionRuntimeCoordinator m_sessionRuntimeCoordinator;
    TurnLifecycleCoordinator m_turnLifecycleCoordinator;
    PanelActionCoordinator m_panelActionCoordinator;
    MultiplayerRuntimeCoordinator m_multiplayerRuntimeCoordinator;
    BuildOverlayCache m_buildOverlayCache;
    WeatherMaskCache m_weatherMaskCache;
    mutable PendingTurnValidationCache m_pendingTurnValidationCache;

#ifdef _WIN32
    WNDPROC m_originalWndProc = nullptr;
    bool m_isInNativeSizeMove = false;
#endif
};
