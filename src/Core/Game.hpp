#pragma once
#include <SFML/Graphics.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/AllWidgets.hpp>
#include <vector>
#include <string>

#include "Core/GameState.hpp"
#include "Core/GameClock.hpp"
#include "Config/GameConfig.hpp"
#include "Config/AIConfig.hpp"
#include "Board/Board.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Buildings/Building.hpp"
#include "Units/PieceFactory.hpp"
#include "Buildings/BuildingFactory.hpp"
#include "Systems/TurnSystem.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/CheckSystem.hpp"
#include "AI/AIController.hpp"
#include "Input/InputHandler.hpp"
#include "Render/Camera.hpp"
#include "Render/Renderer.hpp"
#include "Assets/AssetManager.hpp"
#include "UI/UIManager.hpp"
#include "Save/SaveManager.hpp"

class Game {
public:
    Game();
    void run();

private:
    void init();
    void handleInput();
    void update();
    void render();

    void startNewGame(const std::string& gameName);
    void loadGame(const std::string& saveName);
    void saveGame();
    void commitPlayerTurn();
    void resetPlayerTurn();

    void setupUICallbacks();
    void updateUIState();

    // SFML / TGUI
    sf::RenderWindow m_window;
    tgui::Gui m_gui;

    // State
    GameState m_state;
    GameClock m_clock;
    std::string m_gameName;

    // Config
    GameConfig m_config;
    AIConfig m_aiConfig;

    // Game data
    Board m_board;
    Kingdom m_whiteKingdom;
    Kingdom m_blackKingdom;
    std::vector<Building> m_publicBuildings;
    PieceFactory m_pieceFactory;
    BuildingFactory m_buildingFactory;

    // Systems
    TurnSystem m_turnSystem;
    EventLog m_eventLog;

    // AI
    AIController m_ai;

    // Input/Render/UI
    InputHandler m_input;
    Camera m_camera;
    Renderer m_renderer;
    AssetManager m_assets;
    UIManager m_uiManager;
    SaveManager m_saveManager;
};
