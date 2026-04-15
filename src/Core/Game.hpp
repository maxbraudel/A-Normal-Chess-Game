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
#include "Core/TurnPhase.hpp"
#include "Core/GameClock.hpp"
#include "Config/GameConfig.hpp"
#include "Config/AIConfig.hpp"
#include "Debug/GameStateDebugRecorder.hpp"
#include "Board/Board.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Buildings/Building.hpp"
#include "Units/PieceFactory.hpp"
#include "Buildings/BuildingFactory.hpp"
#include "Systems/TurnSystem.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/CheckSystem.hpp"
#include "AI/AIController.hpp"
#include "AI/AIDirector.hpp"
#include "Input/InputHandler.hpp"
#include "Render/Camera.hpp"
#include "Render/Renderer.hpp"
#include "Assets/AssetManager.hpp"
#include "UI/UIManager.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Save/SaveManager.hpp"

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
    std::string participantName(KingdomId id) const;
    std::string activeTurnLabel() const;

    void setupUICallbacks();
    void updateUIState();

#ifdef _WIN32
    friend LRESULT CALLBACK GameWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void installWindowProcHook();
    void handleNativeResize(sf::Vector2u newSize);
#endif

    // ---- Kingdom helpers (generic, side-agnostic) ----
    Kingdom&       kingdom(KingdomId id)       { return m_kingdoms[kingdomIndex(id)]; }
    const Kingdom& kingdom(KingdomId id) const { return m_kingdoms[kingdomIndex(id)]; }

    Kingdom&       activeKingdom()       { return kingdom(m_turnSystem.getActiveKingdom()); }
    const Kingdom& activeKingdom() const { return kingdom(m_turnSystem.getActiveKingdom()); }

    Kingdom&       enemyKingdom()       { return kingdom(opponent(m_turnSystem.getActiveKingdom())); }
    const Kingdom& enemyKingdom() const { return kingdom(opponent(m_turnSystem.getActiveKingdom())); }

    bool isHumanControlled(KingdomId id) const { return m_controllers[kingdomIndex(id)] == ControllerType::Human; }
    bool isActiveHuman() const { return isHumanControlled(m_turnSystem.getActiveKingdom()); }
    bool isActiveAI()    const { return !isActiveHuman(); }

    KingdomId humanKingdomId() const;

    // SFML / TGUI
    LiveResizeRenderWindow m_window;
    tgui::Gui m_gui;
    sf::View m_hudView;
    sf::Vector2u m_windowSize{1280u, 720u};

    // State
    GameState m_state;
    TurnPhase m_turnPhase = TurnPhase::WhiteTurn;
    GameClock m_clock;
    std::string m_gameName;
    std::array<std::string, kNumKingdoms> m_participantNames;

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

    // Game data
    Board m_board;
    std::array<Kingdom, kNumKingdoms> m_kingdoms;
    std::array<ControllerType, kNumKingdoms> m_controllers;
    std::vector<Building> m_publicBuildings;
    PieceFactory m_pieceFactory;
    BuildingFactory m_buildingFactory;

    // Systems
    TurnSystem m_turnSystem;
    EventLog m_eventLog;
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

#ifdef _WIN32
    WNDPROC m_originalWndProc = nullptr;
    bool m_isInNativeSizeMove = false;
#endif
};
