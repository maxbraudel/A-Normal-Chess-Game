#pragma once
#include "Core/LiveResizeRenderWindow.hpp"
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <ctime>
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
#include "Config/AIConfig.hpp"
#include "Debug/GameStateDebugRecorder.hpp"
#include "Systems/CheckSystem.hpp"
#include "AI/AIController.hpp"
#include "AI/AIDirector.hpp"
#include "Input/InputHandler.hpp"
#include "Render/Camera.hpp"
#include "Render/Renderer.hpp"
#include "Assets/AssetManager.hpp"
#include "UI/UIManager.hpp"
#include "Core/LocalPlayerContext.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Multiplayer/MultiplayerClient.hpp"
#include "Multiplayer/MultiplayerServer.hpp"
#include "Save/SaveManager.hpp"
#include "Systems/CheckResponseRules.hpp"

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
    void startAITurnIfNeeded();
    void pollAITurn();
    void discardPendingAITurn();
    void refreshTurnPhase();
    void stopMultiplayer();
    bool startMultiplayerHostIfNeeded(const GameSessionConfig& session, std::string* errorMessage = nullptr);
    bool joinMultiplayer(const JoinMultiplayerRequest& request, std::string* errorMessage = nullptr);
    bool joinMultiplayerInternal(const JoinMultiplayerRequest& request,
                                 bool preserveLanClientContext,
                                 std::string* errorMessage = nullptr);
    bool reconnectToMultiplayerHost(std::string* errorMessage = nullptr);
    void updateMultiplayer();
    void processMultiplayerServerEvent(const MultiplayerServer::Event& event);
    void processMultiplayerClientEvent(const MultiplayerClient::Event& event);
    bool submitClientTurn(std::string* errorMessage = nullptr);
    bool applyRemoteTurnSubmission(const std::vector<TurnCommand>& commands, std::string* errorMessage = nullptr);
    void commitAuthoritativeTurn();
    bool pushSnapshotToRemote(std::string* errorMessage = nullptr);
    void prepareForClientConnectionAttempt(bool preserveLanClientContext);
    void cacheReconnectRequest(const JoinMultiplayerRequest& request);
    void clearReconnectState();
    void showLanClientDisconnectAlert(const std::string& title,
                                      const std::string& message);
    void centerCameraOnKingdom(KingdomId kingdom);
    void configureLocalPlayerContext(const GameSessionConfig& session);
    bool isLocalPlayerTurn() const;
    bool canLocalPlayerIssueCommands() const;
    KingdomId localPerspectiveKingdom() const;
    bool isMultiplayerSessionReady() const;
    void updateMultiplayerPresentation();
    void returnToMainMenu();
    void openInGameMenu();
    void closeInGameMenu();
    void toggleInGameMenu();
    bool isInGameMenuOpen() const;
    GameMenuPresentation buildGameMenuPresentation() const;
    bool isLanHost() const { return m_localPlayerContext.mode == LocalSessionMode::LanHost; }
    bool isLanClient() const { return m_localPlayerContext.mode == LocalSessionMode::LanClient; }
    std::string participantName(KingdomId id) const;
    std::string activeTurnLabel() const;

    void setupUICallbacks();
    void updateUIState();
    CheckTurnValidation validateActivePendingTurn() const;
    bool isActiveKingInCheckForRules() const;
    bool canQueueNonMoveActions() const;
    InteractionPermissions currentInteractionPermissions(const CheckTurnValidation* validation = nullptr) const;
    InputContext buildInputContext(const InteractionPermissions& permissions);
    bool shouldUseTurnDraft() const;
    void invalidateTurnDraft();
    void ensureTurnDraftUpToDate();
    InputSelectionBookmark captureSelectionBookmark() const;
    void reconcileSelectionBookmark(const InputSelectionBookmark& bookmark);
    Piece* findPieceById(int pieceId);
    Building* findBuildingById(int buildingId);
    Building* findBuildingForBookmark(const InputSelectionBookmark& bookmark);
    void activateSelectTool();
    Board& displayedBoard();
    const Board& displayedBoard() const;
    std::array<Kingdom, kNumKingdoms>& displayedKingdoms();
    const std::array<Kingdom, kNumKingdoms>& displayedKingdoms() const;
    std::vector<Building>& displayedPublicBuildings();
    const std::vector<Building>& displayedPublicBuildings() const;
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

    struct AsyncAITaskState {
        std::mutex mutex;
        bool ready = false;
        std::uint64_t generation = 0;
        KingdomId activeKingdom = KingdomId::Black;
        int turnNumber = 0;
        AITurnPlan plan;
        AIDirectorPlan directorPlan;
        bool usedDirector = false;
    };

    std::shared_ptr<AsyncAITaskState> m_aiTask;
    std::atomic<std::uint64_t> m_aiTaskGeneration{0};

    // Config
    GameConfig m_config;
    AIConfig m_aiConfig;

    GameEngine m_engine;
    TurnDraft m_turnDraft;
    std::uint64_t m_lastTurnDraftRevision = 0;
    GameStateDebugRecorder m_debugRecorder;

    // AI
    AIController m_ai;
    AIDirector m_aiDirector;
    bool m_useNewAI = true;

    // Input/Render/UI
    InputHandler m_input;
    Camera m_camera;
    Renderer m_renderer;
    AssetManager m_assets;
    UIManager m_uiManager;
    SaveManager m_saveManager;
    struct ClientReconnectState {
        bool available = false;
        bool awaitingReconnect = false;
        bool reconnectAttemptInProgress = false;
        JoinMultiplayerRequest request;
        std::string lastErrorMessage;
    };

    ClientReconnectState m_clientReconnectState;
    LocalPlayerContext m_localPlayerContext;
    MultiplayerServer m_multiplayerServer;
    MultiplayerClient m_multiplayerClient;
    bool m_lanHostRemoteSessionEstablished = false;
    bool m_waitingForRemoteTurnResult = false;
    std::string m_multiplayerHostJoinHint;

#ifdef _WIN32
    WNDPROC m_originalWndProc = nullptr;
    bool m_isInNativeSizeMove = false;
#endif
};
