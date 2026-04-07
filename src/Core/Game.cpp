#include "Core/Game.hpp"
#include "Board/BoardGenerator.hpp"
#include "Render/OverlayRenderer.hpp"
#include "Save/SaveData.hpp"
#include "Buildings/BuildingType.hpp"
#include "Systems/BuildSystem.hpp"
#include <algorithm>
#include <iostream>
#include <thread>
#include <ctime>

#ifdef _WIN32
namespace {
Game* s_windowProcGame = nullptr;

const char* pieceTypeName(PieceType type) {
    switch (type) {
        case PieceType::Pawn: return "Pawn";
        case PieceType::Knight: return "Knight";
        case PieceType::Bishop: return "Bishop";
        case PieceType::Rook: return "Rook";
        case PieceType::Queen: return "Queen";
        case PieceType::King: return "King";
    }
    return "Unknown";
}

const char* buildingTypeName(BuildingType type) {
    switch (type) {
        case BuildingType::Church: return "Church";
        case BuildingType::Mine: return "Mine";
        case BuildingType::Farm: return "Farm";
        case BuildingType::Barracks: return "Barracks";
        case BuildingType::WoodWall: return "WoodWall";
        case BuildingType::StoneWall: return "StoneWall";
        case BuildingType::Bridge: return "Bridge";
        case BuildingType::Arena: return "Arena";
    }
    return "Unknown";
}

const char* kingdomName(KingdomId id) {
    return id == KingdomId::White ? "White" : "Black";
}

const char* turnCommandName(TurnCommand::Type type) {
    switch (type) {
        case TurnCommand::Move: return "Move";
        case TurnCommand::Build: return "Build";
        case TurnCommand::Produce: return "Produce";
        case TurnCommand::Upgrade: return "Upgrade";
        case TurnCommand::Marry: return "Marry";
        case TurnCommand::FormGroup: return "FormGroup";
        case TurnCommand::BreakGroup: return "BreakGroup";
    }
    return "Unknown";
}

struct AIWorldSnapshot {
    Board board;
    std::array<Kingdom, kNumKingdoms> kingdoms;
    std::vector<Building> publicBuildings;
};

void relinkBoardSnapshot(Board& board,
                         std::array<Kingdom, kNumKingdoms>& kingdoms,
                         std::vector<Building>& publicBuildings) {
    const int diameter = board.getDiameter();
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            Cell& cell = board.getCell(x, y);
            cell.piece = nullptr;
            cell.building = nullptr;
        }
    }

    for (auto& building : publicBuildings) {
        for (const auto& pos : building.getOccupiedCells()) {
            if (board.isInBounds(pos.x, pos.y)) {
                board.getCell(pos.x, pos.y).building = &building;
            }
        }
    }

    for (auto& kingdom : kingdoms) {
        for (auto& building : kingdom.buildings) {
            for (const auto& pos : building.getOccupiedCells()) {
                if (board.isInBounds(pos.x, pos.y)) {
                    board.getCell(pos.x, pos.y).building = &building;
                }
            }
        }
        for (auto& piece : kingdom.pieces) {
            if (board.isInBounds(piece.position.x, piece.position.y)) {
                board.getCell(piece.position.x, piece.position.y).piece = &piece;
            }
        }
    }
}

AIWorldSnapshot makeAIWorldSnapshot(const Board& board,
                                    const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                    const std::vector<Building>& publicBuildings) {
    AIWorldSnapshot snapshot{board, kingdoms, publicBuildings};
    relinkBoardSnapshot(snapshot.board, snapshot.kingdoms, snapshot.publicBuildings);
    return snapshot;
}
}

LRESULT CALLBACK GameWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_ERASEBKGND && s_windowProcGame && s_windowProcGame->m_isInNativeSizeMove) {
        return 1;
    }

    if (!s_windowProcGame || !s_windowProcGame->m_originalWndProc) {
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    const LRESULT result = CallWindowProc(s_windowProcGame->m_originalWndProc,
                                          hwnd, message, wParam, lParam);

    switch (message) {
        case WM_ENTERSIZEMOVE:
            s_windowProcGame->m_isInNativeSizeMove = true;
            break;

        case WM_EXITSIZEMOVE:
            s_windowProcGame->m_isInNativeSizeMove = false;
            break;

        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                const sf::Vector2u newSize(static_cast<unsigned int>(LOWORD(lParam)),
                                           static_cast<unsigned int>(HIWORD(lParam)));
                s_windowProcGame->handleNativeResize(newSize);

                if (s_windowProcGame->m_isInNativeSizeMove && s_windowProcGame->m_window.isOpen()) {
                    s_windowProcGame->render();
                }
            }
            break;

        default:
            break;
    }

    return result;
}
#endif

Game::Game()
    : m_state(GameState::MainMenu),
      m_kingdoms{Kingdom(KingdomId::White), Kingdom(KingdomId::Black)},
      m_controllers{ControllerType::Human, ControllerType::AI} {}

KingdomId Game::humanKingdomId() const {
    for (int i = 0; i < kNumKingdoms; ++i)
        if (m_controllers[i] == ControllerType::Human)
            return m_kingdoms[i].id;
    return KingdomId::White; // fallback
}

void Game::run() {
    init();
    while (m_window.isOpen()) {
        m_clock.update();
        handleInput();

#ifdef _WIN32
        if (m_isInNativeSizeMove) {
            continue;
        }
#endif

        update();
        render();
    }
}

void Game::init() {
    // Load config
    m_config.loadFromFile("assets/config/game_params.json");
    m_aiConfig.loadFromFile("assets/config/ai_params.json");
    m_ai.loadConfig("assets/config/ai_params.json");
    m_aiDirector.loadConfig("assets/config/ai_params.json");
    m_useNewAI = m_aiConfig.useNewAI;

    // Create window
    m_window.create(sf::VideoMode(1280, 720), "A Normal Chess Game", sf::Style::Default);
    m_window.setFramerateLimit(60);
    m_windowSize = m_window.getSize();

#ifdef _WIN32
    installWindowProcHook();
#endif

    m_gui.setTarget(m_window);

    // Load assets
    m_assets.loadAll("assets");

    // Init renderer
    m_renderer.init(m_assets, m_config.getCellSizePx());

    // Init camera
    m_camera.init(m_window);
    handleWindowResize(m_windowSize);

    // Init UI
    m_uiManager.init(m_gui, m_assets);
    setupUICallbacks();

    // Show main menu
    m_state = GameState::MainMenu;
    m_uiManager.showMainMenu();
}

void Game::handleWindowResize(sf::Vector2u newSize) {
    if (newSize.x == 0 || newSize.y == 0) {
        return;
    }

    m_windowSize = newSize;
    m_window.forceResizeCache(newSize);
    m_camera.handleWindowResize(newSize);

    m_hudView = sf::View(sf::FloatRect(0.f, 0.f,
        static_cast<float>(newSize.x), static_cast<float>(newSize.y)));

    const tgui::FloatRect guiRect(0.f, 0.f,
        static_cast<float>(newSize.x), static_cast<float>(newSize.y));
    m_gui.setAbsoluteViewport(guiRect);
    m_gui.setAbsoluteView(guiRect);
}

#ifdef _WIN32
void Game::installWindowProcHook() {
    s_windowProcGame = this;
    const HWND hwnd = m_window.getSystemHandle();
    m_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(GameWindowProc)));
}

void Game::handleNativeResize(sf::Vector2u newSize) {
    handleWindowResize(newSize);
}
#endif

void Game::handleInput() {
    sf::Event event;
    while (m_window.pollEvent(event)) {
        m_gui.handleEvent(event);

        if (event.type == sf::Event::Closed) {
            m_window.close();
            return;
        }

        if (event.type == sf::Event::Resized) {
            sf::Vector2u newSize(event.size.width, event.size.height);
            handleWindowResize(newSize);
            continue;
        }

        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
            if (m_state == GameState::Playing) {
                m_state = GameState::Paused;
                m_uiManager.showPauseMenu();
            } else if (m_state == GameState::Paused) {
                m_state = GameState::Playing;
                m_uiManager.hidePauseMenu();
            }
        }

        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::P) {
            if (m_state == GameState::Playing || m_state == GameState::Paused || m_state == GameState::GameOver) {
                m_debugRecorder.exportHistory(m_gameName,
                                             m_turnSystem.getTurnNumber(),
                                             m_turnSystem.getActiveKingdom());
            }
        }

        // Spacebar acts as the Play button (commit turn)
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Space) {
            if (m_state == GameState::Playing && m_turnPhase == TurnPhase::WhiteTurn && isActiveHuman()) {
                commitPlayerTurn();
            }
        }

        if (m_state == GameState::Playing) {
            const bool allowCommands = (m_turnPhase == TurnPhase::WhiteTurn && isActiveHuman());
            Kingdom& selectableKingdom = allowCommands ? activeKingdom() : kingdom(humanKingdomId());
            Kingdom& opposingKingdom = allowCommands ? enemyKingdom() : kingdom(opponent(humanKingdomId()));
            m_input.handleEvent(event, m_window, m_camera, m_board, m_turnSystem,
                                 selectableKingdom, opposingKingdom, m_publicBuildings,
                                 m_uiManager, m_config, allowCommands);
        }
    }
}

void Game::update() {
    switch (m_state) {
        case GameState::Playing: {
            if (m_turnPhase == TurnPhase::BlackTurn) {
                startAITurnIfNeeded();
                pollAITurn();
            }

            updateUIState();
            m_uiManager.update();
            break;
        }
        case GameState::GameOver:
            break;
        default:
            break;
    }
}

void Game::render() {
    m_window.clear(sf::Color(30, 30, 30));

    if (m_state == GameState::Playing || m_state == GameState::Paused || m_state == GameState::GameOver) {
        m_camera.applyTo(m_window);
        m_renderer.setSkipPieceId(m_input.getCapturePreviewPieceId());
        m_renderer.drawWorldBase(m_window, m_camera, m_board, m_kingdoms,
            m_publicBuildings);

        const bool showActionOverlays = (m_turnPhase == TurnPhase::WhiteTurn && isActiveHuman());

        if (showActionOverlays && m_input.getCurrentTool() == ToolState::Select && m_input.getSelectedPiece()) {
            sf::Vector2i highlightedOrigin = m_input.hasMovePreview()
                ? m_input.getMoveFrom()
                : m_input.getSelectedPiece()->position;
            m_renderer.getOverlay().drawOriginCell(m_window, m_camera,
                highlightedOrigin, m_config.getCellSizePx());
            m_renderer.getOverlay().drawReachableCells(m_window, m_camera,
                m_input.getValidMoves(), m_config.getCellSizePx());
            if (!m_input.getDangerMoves().empty()) {
                m_renderer.getOverlay().drawDangerCells(m_window, m_camera,
                    m_input.getDangerMoves(), m_config.getCellSizePx());
            }
        }

        m_renderer.drawPiecesLayer(m_window, m_camera, m_kingdoms);

        // Draw overlays based on input state
        if (m_input.getCurrentTool() == ToolState::Select) {
            if (m_input.getSelectedPiece()) {
                m_renderer.getOverlay().drawSelectionFrame(m_window, m_camera,
                    m_hudView, m_windowSize, m_input.getSelectedPiece()->position,
                    1, 1, m_config.getCellSizePx());
            }
            if (m_input.getSelectedBuilding()) {
                const Building* selectedBuilding = m_input.getSelectedBuilding();
                m_renderer.getOverlay().drawSelectionFrame(m_window, m_camera,
                    m_hudView, m_windowSize, selectedBuilding->origin,
                    selectedBuilding->width, selectedBuilding->height,
                    m_config.getCellSizePx());
            }
        }
        if (showActionOverlays && m_input.hasBuildPreview()) {
            BuildingType bt = m_input.getBuildPreviewType();
            int bw = m_config.getBuildingWidth(bt);
            int bh = m_config.getBuildingHeight(bt);
            Kingdom& human = kingdom(humanKingdomId());
            Piece* king = human.getKing();
            bool valid = king && BuildSystem::canBuild(bt, m_input.getBuildPreviewOrigin(),
                                                        *king, m_board, human, m_config);
            m_renderer.getOverlay().drawBuildPreview(m_window, m_camera,
                m_input.getBuildPreviewOrigin(), bw, bh, m_config.getCellSizePx(), valid);
        }
        if (showActionOverlays) {
            if (const TurnCommand* pendingBuild = m_turnSystem.getPendingBuildCommand()) {
            int bw = m_config.getBuildingWidth(pendingBuild->buildingType);
            int bh = m_config.getBuildingHeight(pendingBuild->buildingType);
            m_renderer.getOverlay().drawBuildPreview(m_window, m_camera,
                pendingBuild->buildOrigin, bw, bh, m_config.getCellSizePx(), true);
            }
        }
        m_renderer.getOverlay().drawZoneIndicators(m_window, m_camera, m_hudView,
            m_windowSize, m_board,
            m_publicBuildings, m_kingdoms, m_config.getCellSizePx(), m_assets);
    }

    m_window.setView(m_hudView);
    m_gui.draw();
    m_window.display();
}

void Game::startNewGame(const std::string& gameName) {
    discardPendingAITurn();
    m_gameName = gameName;

    // Init board
    m_board.init(m_config.getMapRadius());
    m_publicBuildings.clear();
    auto spawnResult = BoardGenerator::generate(m_board, m_config, m_publicBuildings);

    // Init kingdoms
    for (int i = 0; i < kNumKingdoms; ++i) {
        KingdomId id = static_cast<KingdomId>(i);
        m_kingdoms[i] = Kingdom(id);
        m_kingdoms[i].gold = m_config.getStartingGold();
    }

    // Create initial kings
    Piece whiteKing = m_pieceFactory.createPiece(PieceType::King, KingdomId::White, spawnResult.playerSpawn);
    kingdom(KingdomId::White).addPiece(whiteKing);
    m_board.getCell(spawnResult.playerSpawn.x, spawnResult.playerSpawn.y).piece = &kingdom(KingdomId::White).pieces.back();

    Piece blackKing = m_pieceFactory.createPiece(PieceType::King, KingdomId::Black, spawnResult.aiSpawn);
    kingdom(KingdomId::Black).addPiece(blackKing);
    m_board.getCell(spawnResult.aiSpawn.x, spawnResult.aiSpawn.y).piece = &kingdom(KingdomId::Black).pieces.back();

    // Init turn system
    m_turnSystem = TurnSystem();
    m_turnSystem.setActiveKingdom(KingdomId::White);

    // Event log
    m_eventLog.clear();
    m_eventLog.log(1, KingdomId::White, "Game started: " + gameName);
    m_debugRecorder.reset();
    m_debugRecorder.recordSnapshot(m_turnSystem.getTurnNumber(),
                                   m_turnSystem.getActiveKingdom(),
                                   m_kingdoms,
                                   "initial_state_new_game");

    // Center camera on player spawn
    float cx = static_cast<float>(spawnResult.playerSpawn.x * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
    float cy = static_cast<float>(spawnResult.playerSpawn.y * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
    m_camera.centerOn({cx, cy});

    // Switch to playing
    m_state = GameState::Playing;
    refreshTurnPhase();
    m_uiManager.showHUD();
}

void Game::loadGame(const std::string& saveName) {
    discardPendingAITurn();
    SaveData data;
    std::string path = "saves/" + saveName + ".json";
    if (!m_saveManager.load(path, data)) {
        std::cerr << "Failed to load save: " << path << std::endl;
        return;
    }

    m_gameName = data.gameName;

    // Restore board
    m_board.init(data.mapRadius);
    // If grid data was saved, restore it
    if (!data.grid.empty()) {
        auto& grid = m_board.getGrid();
        int diam = m_board.getDiameter();
        for (int y = 0; y < diam && y < static_cast<int>(data.grid.size()); ++y) {
            for (int x = 0; x < diam && x < static_cast<int>(data.grid[y].size()); ++x) {
                grid[y][x].type = data.grid[y][x].type;
                grid[y][x].isInCircle = data.grid[y][x].isInCircle;
            }
        }
    } else {
        // Regenerate map (simplified restore — full restore requires grid save)
        m_publicBuildings.clear();
        BoardGenerator::generate(m_board, m_config, m_publicBuildings);
    }

    // Restore kingdoms
    for (int i = 0; i < kNumKingdoms; ++i) {
        KingdomId id = static_cast<KingdomId>(i);
        m_kingdoms[i] = Kingdom(id);
        m_kingdoms[i].gold = data.kingdoms[i].gold;
        for (const auto& p : data.kingdoms[i].pieces) m_kingdoms[i].addPiece(p);
        for (const auto& b : data.kingdoms[i].buildings) m_kingdoms[i].addBuilding(b);
    }

    m_publicBuildings = data.publicBuildings;

    // Sync board cell piece pointers (pieces stored in kingdom, board needs raw ptrs)
    for (int i = 0; i < kNumKingdoms; ++i) {
        for (auto& p : m_kingdoms[i].pieces) {
            if (m_board.isInBounds(p.position.x, p.position.y))
                m_board.getCell(p.position.x, p.position.y).piece = &p;
        }
    }

    // Restore turn system
    m_turnSystem = TurnSystem();
    m_turnSystem.setActiveKingdom(data.activeKingdom);

    // Restore events
    m_eventLog.clear();
    for (const auto& e : data.events) m_eventLog.log(e.turnNumber, e.kingdom, e.message);
    m_debugRecorder.reset();
    m_debugRecorder.recordSnapshot(m_turnSystem.getTurnNumber(),
                                   m_turnSystem.getActiveKingdom(),
                                   m_kingdoms,
                                   "initial_state_loaded_game");

    // Center camera on human player's king
    Piece* king = kingdom(humanKingdomId()).getKing();
    if (king) {
        float cx = static_cast<float>(king->position.x * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
        float cy = static_cast<float>(king->position.y * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
        m_camera.centerOn({cx, cy});
    }

    m_state = GameState::Playing;
    refreshTurnPhase();
    m_uiManager.showHUD();
}

void Game::saveGame() {
    SaveData data;
    data.gameName = m_gameName;
    data.turnNumber = m_turnSystem.getTurnNumber();
    data.activeKingdom = m_turnSystem.getActiveKingdom();
    data.mapRadius = m_board.getRadius();

    // Grid (only save cell types and circle mask)
    int diam = m_board.getDiameter();
    data.grid.resize(diam);
    for (int y = 0; y < diam; ++y) {
        data.grid[y].resize(diam);
        for (int x = 0; x < diam; ++x) {
            const Cell& cell = m_board.getCell(x, y);
            data.grid[y][x].type = cell.type;
            data.grid[y][x].isInCircle = cell.isInCircle;
        }
    }

    for (int i = 0; i < kNumKingdoms; ++i) {
        data.kingdoms[i].id = static_cast<KingdomId>(i);
        data.kingdoms[i].gold = m_kingdoms[i].gold;
        data.kingdoms[i].pieces.assign(m_kingdoms[i].pieces.begin(), m_kingdoms[i].pieces.end());
        data.kingdoms[i].buildings = m_kingdoms[i].buildings;
    }

    data.publicBuildings = m_publicBuildings;
    data.events = m_eventLog.getEvents();

    std::string path = "saves/" + m_gameName + ".json";
    if (!m_saveManager.save(path, data)) {
        std::cerr << "Failed to save game!" << std::endl;
    }
}

void Game::commitPlayerTurn() {
    KingdomId activeId = m_turnSystem.getActiveKingdom();
    KingdomId enemyId  = opponent(activeId);

    m_turnSystem.commitTurn(m_board, activeKingdom(), enemyKingdom(),
                             m_publicBuildings, m_config, m_eventLog,
                             m_pieceFactory, m_buildingFactory);
    m_debugRecorder.logTurnState(m_turnSystem.getTurnNumber(), m_kingdoms, "after player commit");
    m_debugRecorder.recordSnapshot(m_turnSystem.getTurnNumber(),
                                   m_turnSystem.getActiveKingdom(),
                                   m_kingdoms,
                                   "after_player_commit");

    // Check if opponent is in checkmate
    if (CheckSystem::isCheckmate(enemyId, m_board, m_config)) {
        m_state = GameState::GameOver;
        std::string winner = (activeId == KingdomId::White) ? "White" : "Black";
        m_eventLog.log(m_turnSystem.getTurnNumber(), activeId,
                        "Checkmate! " + winner + " wins!");
        m_input.clearMovePreview();
        m_input.clearSelection();
        return;
    }

    m_turnSystem.advanceTurn();
    m_input.clearMovePreview();
    m_input.clearSelection();
    refreshTurnPhase();
    startAITurnIfNeeded();
}

void Game::resetPlayerTurn() {
    m_input.cancelLiveMove();  // restore piece position if a live move is pending
    m_turnSystem.resetPendingCommands();
    m_input.clearSelection();
}

void Game::setupUICallbacks() {
    // Pause menu
    m_uiManager.pauseMenu().setOnResume([this]() {
        m_state = GameState::Playing;
        m_uiManager.hidePauseMenu();
    });
    m_uiManager.pauseMenu().setOnSave([this]() {
        saveGame();
    });
    m_uiManager.pauseMenu().setOnQuitToMenu([this]() {
        discardPendingAITurn();
        m_state = GameState::MainMenu;
        m_uiManager.hidePauseMenu();
        m_uiManager.showMainMenu();
    });

    // Main menu
    m_uiManager.mainMenu().setOnNewGame([this]() {
        startNewGame("Game_" + std::to_string(std::time(nullptr)));
    });
    m_uiManager.mainMenu().setOnContinue([this]() {
        auto saves = m_saveManager.listSaves("saves");
        if (!saves.empty()) {
            loadGame(saves.back()); // Load most recent
        }
    });

    // HUD
    m_uiManager.hud().setOnReset([this]() {
        if (m_turnPhase != TurnPhase::WhiteTurn) return;
        resetPlayerTurn();
    });
    m_uiManager.hud().setOnPlay([this]() {
        if (m_turnPhase != TurnPhase::WhiteTurn) return;
        commitPlayerTurn();
    });

    // Toolbar
    m_uiManager.toolBar().setOnSelect([this]() {
        m_input.setTool(ToolState::Select);
        m_uiManager.hideAllPanels();
        m_uiManager.showHUD();
    });
    m_uiManager.toolBar().setOnBuild([this]() {
        if (m_turnPhase != TurnPhase::WhiteTurn) return;
        m_input.setTool(ToolState::Build);
        m_uiManager.showBuildToolPanel(kingdom(humanKingdomId()), m_config);
    });
    m_uiManager.toolBar().setOnLog([this]() {
        m_input.setTool(ToolState::Journal);
        m_uiManager.showEventLogPanel(m_eventLog);
    });

    // Build tool panel
    m_uiManager.buildToolPanel().setOnSelectBuildType([this](int type) {
        if (m_turnPhase != TurnPhase::WhiteTurn) return;
        m_input.setBuildType(static_cast<BuildingType>(type));
    });

    // Piece panel upgrade
    m_uiManager.piecePanel().setOnUpgrade([this](int pieceId) {
        if (m_turnPhase != TurnPhase::WhiteTurn) return;
        Piece* piece = kingdom(humanKingdomId()).getPieceById(pieceId);
        if (!piece) return;
        TurnCommand cmd;
        cmd.type = TurnCommand::Upgrade;
        cmd.upgradePieceId = piece->id;
        // Determine next type
        switch (piece->type) {
            case PieceType::Pawn:   cmd.upgradeTarget = PieceType::Knight; break;
            case PieceType::Knight: cmd.upgradeTarget = PieceType::Bishop; break;
            case PieceType::Bishop: cmd.upgradeTarget = PieceType::Rook;   break;
            case PieceType::Rook:   cmd.upgradeTarget = PieceType::Queen;  break;
            default: return;
        }
        m_turnSystem.queueCommand(cmd);
    });

    // Barracks panel produce
    m_uiManager.barracksPanel().setOnProduce([this](int barracksId, int pieceType) {
        if (m_turnPhase != TurnPhase::WhiteTurn) return;
        TurnCommand cmd;
        cmd.type = TurnCommand::Produce;
        cmd.barracksId = barracksId;
        cmd.produceType = static_cast<PieceType>(pieceType);
        m_turnSystem.queueCommand(cmd);
    });
}

void Game::updateUIState() {
    int gold = activeKingdom().gold;
    m_uiManager.hud().update(m_turnSystem.getTurnNumber(), m_turnPhase, gold);
    const bool allowCommands = (m_turnPhase == TurnPhase::WhiteTurn);

    // Show contextual panels
    if (m_input.getSelectedPiece()) {
        m_uiManager.showPiecePanel(*m_input.getSelectedPiece(), m_config, allowCommands);
    } else if (m_input.getSelectedBuilding()) {
        Building* bld = m_input.getSelectedBuilding();
        if (bld->type == BuildingType::Barracks && bld->owner == humanKingdomId()) {
            m_uiManager.showBarracksPanel(*bld, kingdom(humanKingdomId()), m_config, allowCommands);
        } else {
            m_uiManager.showBuildingPanel(*bld);
        }
    }
}

void Game::refreshTurnPhase() {
    m_turnPhase = (m_turnSystem.getActiveKingdom() == KingdomId::White)
        ? TurnPhase::WhiteTurn
        : TurnPhase::BlackTurn;
}

void Game::discardPendingAITurn() {
    m_aiTaskGeneration.fetch_add(1, std::memory_order_relaxed);
    m_aiTask.reset();
}

void Game::startAITurnIfNeeded() {
    if (m_state != GameState::Playing || m_turnPhase != TurnPhase::BlackTurn || !isActiveAI()) {
        return;
    }
    if (m_aiTask) {
        return;
    }

    const KingdomId activeId = m_turnSystem.getActiveKingdom();
    const int turnNumber = m_turnSystem.getTurnNumber();
    const std::uint64_t generation = m_aiTaskGeneration.load(std::memory_order_relaxed);

    auto task = std::make_shared<AsyncAITaskState>();
    task->generation = generation;
    task->activeKingdom = activeId;
    task->turnNumber = turnNumber;
    m_aiTask = task;

    AIWorldSnapshot snapshot = makeAIWorldSnapshot(m_board, m_kingdoms, m_publicBuildings);
    GameConfig configCopy = m_config;

    if (m_useNewAI) {
        AIDirector directorWorker = m_aiDirector;
        std::thread([task, snapshot = std::move(snapshot), directorWorker = std::move(directorWorker), configCopy]() mutable {
            Kingdom& self = snapshot.kingdoms[kingdomIndex(task->activeKingdom)];
            Kingdom& enemy = snapshot.kingdoms[kingdomIndex(opponent(task->activeKingdom))];
            AIDirectorPlan plan = directorWorker.computeTurn(snapshot.board, self, enemy,
                snapshot.publicBuildings, task->turnNumber, configCopy);
            std::scoped_lock lock(task->mutex);
            task->directorPlan = std::move(plan);
            task->usedDirector = true;
            task->ready = true;
        }).detach();
    } else {
        AIController aiWorker = m_ai;
        std::thread([task, snapshot = std::move(snapshot), aiWorker = std::move(aiWorker), configCopy]() mutable {
            Kingdom& self = snapshot.kingdoms[kingdomIndex(task->activeKingdom)];
            Kingdom& enemy = snapshot.kingdoms[kingdomIndex(opponent(task->activeKingdom))];
            AITurnPlan plan = aiWorker.computeTurnPlan(snapshot.board, self, enemy,
                snapshot.publicBuildings, task->turnNumber, configCopy);
            std::scoped_lock lock(task->mutex);
            task->plan = std::move(plan);
            task->usedDirector = false;
            task->ready = true;
        }).detach();
    }
}

void Game::pollAITurn() {
    if (!m_aiTask) {
        return;
    }

    bool ready = false;
    {
        std::scoped_lock lock(m_aiTask->mutex);
        ready = m_aiTask->ready;
    }
    if (!ready) {
        return;
    }

    std::shared_ptr<AsyncAITaskState> task = m_aiTask;
    m_aiTask.reset();

    const std::uint64_t currentGeneration = m_aiTaskGeneration.load(std::memory_order_relaxed);
    if (task->generation != currentGeneration
        || m_state != GameState::Playing
        || m_turnSystem.getActiveKingdom() != task->activeKingdom
        || m_turnSystem.getTurnNumber() != task->turnNumber) {
        return;
    }

    AITurnPlan plan;
    AIDirectorPlan directorPlan;
    bool usedDirector = false;
    {
        std::scoped_lock lock(task->mutex);
        usedDirector = task->usedDirector;
        if (usedDirector)
            directorPlan = task->directorPlan;
        else
            plan = task->plan;
    }

    if (usedDirector) {
        m_aiDirector.applyPlanMetadata(directorPlan);
        std::cout << "AI PLAN | objective=" << directorPlan.objectiveName
                  << " | commands=" << directorPlan.commands.size() << '\n';
        for (const auto& cmd : directorPlan.commands) {
            std::cout << "  - " << turnCommandName(cmd.type);
            if (cmd.type == TurnCommand::Move) {
                std::cout << " piece=" << cmd.pieceId
                          << " from=(" << cmd.origin.x << "," << cmd.origin.y << ")"
                          << " to=(" << cmd.destination.x << "," << cmd.destination.y << ")";
            } else if (cmd.type == TurnCommand::Build) {
                std::cout << " building=" << buildingTypeName(cmd.buildingType)
                          << " origin=(" << cmd.buildOrigin.x << "," << cmd.buildOrigin.y << ")";
            } else if (cmd.type == TurnCommand::Produce) {
                std::cout << " barracks=" << cmd.barracksId
                          << " unit=" << pieceTypeName(cmd.produceType);
            }
            std::cout << '\n';
        }
        m_eventLog.log(task->turnNumber, task->activeKingdom, "AI Objective: " + directorPlan.objectiveName);
        for (const auto& cmd : directorPlan.commands) {
            m_turnSystem.queueCommand(cmd);
        }
    } else {
        m_ai.applyTurnPlanMetadata(plan);
        std::cout << "AI PLAN | phase=" << plan.phaseName
                  << " | commands=" << plan.commands.size() << '\n';
        for (const auto& cmd : plan.commands) {
            std::cout << "  - " << turnCommandName(cmd.type);
            if (cmd.type == TurnCommand::Move) {
                std::cout << " piece=" << cmd.pieceId
                          << " from=(" << cmd.origin.x << "," << cmd.origin.y << ")"
                          << " to=(" << cmd.destination.x << "," << cmd.destination.y << ")";
            } else if (cmd.type == TurnCommand::Build) {
                std::cout << " building=" << buildingTypeName(cmd.buildingType)
                          << " origin=(" << cmd.buildOrigin.x << "," << cmd.buildOrigin.y << ")";
            } else if (cmd.type == TurnCommand::Produce) {
                std::cout << " barracks=" << cmd.barracksId
                          << " unit=" << pieceTypeName(cmd.produceType);
            }
            std::cout << '\n';
        }
        m_eventLog.log(task->turnNumber, task->activeKingdom, "AI Phase: " + plan.phaseName);
        for (const auto& cmd : plan.commands) {
            m_turnSystem.queueCommand(cmd);
        }
    }
    m_eventLog.log(task->turnNumber, task->activeKingdom, "AI completed turn planning.");

    const KingdomId activeId = m_turnSystem.getActiveKingdom();
    const KingdomId enemyId = opponent(activeId);
    m_turnSystem.commitTurn(m_board, activeKingdom(), enemyKingdom(),
        m_publicBuildings, m_config, m_eventLog,
        m_pieceFactory, m_buildingFactory);
    m_debugRecorder.logTurnState(m_turnSystem.getTurnNumber(), m_kingdoms, "after ai commit");
    m_debugRecorder.recordSnapshot(m_turnSystem.getTurnNumber(),
                                   m_turnSystem.getActiveKingdom(),
                                   m_kingdoms,
                                   "after_ai_commit");
    m_turnSystem.advanceTurn();
    refreshTurnPhase();

    if (CheckSystem::isCheckmate(enemyId, m_board, m_config)) {
        m_state = GameState::GameOver;
        std::string winner = (activeId == KingdomId::White) ? "White" : "Black";
        m_eventLog.log(m_turnSystem.getTurnNumber(), activeId,
            "Checkmate! " + winner + " wins!");
    }
}
