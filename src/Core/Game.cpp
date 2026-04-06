#include "Core/Game.hpp"
#include "Board/BoardGenerator.hpp"
#include "Render/OverlayRenderer.hpp"
#include "Save/SaveData.hpp"
#include "Buildings/BuildingType.hpp"
#include "Systems/BuildSystem.hpp"
#include <iostream>
#include <ctime>

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
        update();
        render();
    }
}

void Game::init() {
    // Load config
    m_config.loadFromFile("assets/config/game_params.json");
    m_aiConfig.loadFromFile("assets/config/ai_params.json");
    m_ai.loadConfig("assets/config/ai_params.json");

    // Create window
    m_window.create(sf::VideoMode(1280, 720), "A Normal Chess Game", sf::Style::Default);
    m_window.setFramerateLimit(60);

    m_gui.setTarget(m_window);

    // Load assets
    m_assets.loadAll("assets");

    // Init renderer
    m_renderer.init(m_assets, m_config.getCellSizePx());

    // Init camera
    m_camera.init(m_window);

    // Init UI
    m_uiManager.init(m_gui, m_assets);
    setupUICallbacks();

    // Show main menu
    m_state = GameState::MainMenu;
    m_uiManager.showMainMenu();
}

void Game::handleInput() {
    sf::Event event;
    while (m_window.pollEvent(event)) {
        m_gui.handleEvent(event);

        if (event.type == sf::Event::Closed) {
            m_window.close();
            return;
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

        // Spacebar acts as the Play button (commit turn)
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Space) {
            if (m_state == GameState::Playing && isActiveHuman()) {
                commitPlayerTurn();
            }
        }

        if (m_state == GameState::Playing && isActiveHuman()) {
            m_input.handleEvent(event, m_window, m_camera, m_board, m_turnSystem,
                                 activeKingdom(), enemyKingdom(), m_publicBuildings,
                                 m_uiManager, m_config);
        }
    }
}

void Game::update() {
    switch (m_state) {
        case GameState::Playing: {
            // If AI's turn
            if (isActiveAI()) {
                KingdomId activeId = m_turnSystem.getActiveKingdom();
                KingdomId enemyId  = opponent(activeId);

                m_ai.playTurn(m_board, activeKingdom(), enemyKingdom(),
                               m_publicBuildings, m_turnSystem, m_config, m_eventLog);

                // Check if the opponent is in checkmate BEFORE committing
                if (CheckSystem::isCheckmate(enemyId, m_board, m_config)) {
                    m_state = GameState::GameOver;
                    std::string winner = (activeId == KingdomId::White) ? "White" : "Black";
                    m_eventLog.log(m_turnSystem.getTurnNumber(), activeId,
                                    "Checkmate! " + winner + " wins!");
                    break;
                }

                m_turnSystem.commitTurn(m_board, activeKingdom(), enemyKingdom(),
                                         m_publicBuildings, m_config, m_eventLog,
                                         m_pieceFactory, m_buildingFactory);
                m_turnSystem.advanceTurn();

                // Also check after commit in case the committed moves create checkmate
                // (now the active kingdom has changed, so recompute who's the new defender)
                KingdomId newDefender = enemyId;
                if (CheckSystem::isCheckmate(newDefender, m_board, m_config)) {
                    m_state = GameState::GameOver;
                    std::string winner = (activeId == KingdomId::White) ? "White" : "Black";
                    m_eventLog.log(m_turnSystem.getTurnNumber(), activeId,
                                    "Checkmate! " + winner + " wins!");
                }
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
        m_renderer.draw(m_window, m_camera, m_board, m_kingdoms,
                          m_publicBuildings, m_turnSystem);

        // Draw overlays based on input state
        if (m_input.getCurrentTool() == ToolState::Select) {
            if (m_input.getSelectedPiece()) {
                m_renderer.getOverlay().drawSelectedPieceMarker(m_window, m_camera,
                    m_input.getSelectedPiece()->position, m_config.getCellSizePx());
                m_renderer.getOverlay().drawReachableCells(m_window, m_camera,
                    m_input.getValidMoves(), m_config.getCellSizePx());
                // Show red overlay on king moves that are under enemy threat
                if (!m_input.getDangerMoves().empty()) {
                    m_renderer.getOverlay().drawDangerCells(m_window, m_camera,
                        m_input.getDangerMoves(), m_config.getCellSizePx());
                }
            }
            if (m_input.getSelectedPiece() && m_input.hasMovePreview()) {
                m_renderer.getOverlay().drawOriginCell(m_window, m_camera,
                    m_input.getMoveFrom(), m_config.getCellSizePx());
            }
        }
        if (m_input.hasBuildPreview()) {
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
        m_renderer.getOverlay().drawZoneIndicators(m_window, m_camera, m_board,
            m_publicBuildings, m_kingdoms, m_config.getCellSizePx(), m_assets);

        // Reset to default view for GUI
        m_window.setView(m_window.getDefaultView());
    }

    m_gui.draw();
    m_window.display();
}

void Game::startNewGame(const std::string& gameName) {
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

    // Center camera on player spawn
    float cx = static_cast<float>(spawnResult.playerSpawn.x * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
    float cy = static_cast<float>(spawnResult.playerSpawn.y * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
    m_camera.centerOn({cx, cy});

    // Switch to playing
    m_state = GameState::Playing;
    m_uiManager.showHUD();
}

void Game::loadGame(const std::string& saveName) {
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

    // Center camera on human player's king
    Piece* king = kingdom(humanKingdomId()).getKing();
    if (king) {
        float cx = static_cast<float>(king->position.x * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
        float cy = static_cast<float>(king->position.y * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
        m_camera.centerOn({cx, cy});
    }

    m_state = GameState::Playing;
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
        resetPlayerTurn();
    });
    m_uiManager.hud().setOnPlay([this]() {
        commitPlayerTurn();
    });

    // Toolbar
    m_uiManager.toolBar().setOnSelect([this]() {
        m_input.setTool(ToolState::Select);
        m_uiManager.hideAllPanels();
        m_uiManager.showHUD();
    });
    m_uiManager.toolBar().setOnBuild([this]() {
        m_input.setTool(ToolState::Build);
        m_uiManager.showBuildToolPanel(kingdom(humanKingdomId()), m_config);
    });
    m_uiManager.toolBar().setOnLog([this]() {
        m_input.setTool(ToolState::Journal);
        m_uiManager.showEventLogPanel(m_eventLog);
    });

    // Build tool panel
    m_uiManager.buildToolPanel().setOnSelectBuildType([this](int type) {
        m_input.setBuildType(static_cast<BuildingType>(type));
    });

    // Piece panel upgrade
    m_uiManager.piecePanel().setOnUpgrade([this](int pieceId) {
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
        TurnCommand cmd;
        cmd.type = TurnCommand::Produce;
        cmd.barracksId = barracksId;
        cmd.produceType = static_cast<PieceType>(pieceType);
        m_turnSystem.queueCommand(cmd);
    });
}

void Game::updateUIState() {
    // Update HUD
    std::string activePlayer = (m_turnSystem.getActiveKingdom() == KingdomId::White) ? "White" : "Black";
    int gold = activeKingdom().gold;
    m_uiManager.hud().update(m_turnSystem.getTurnNumber(), activePlayer, gold);

    // Show contextual panels
    if (m_input.getSelectedPiece()) {
        m_uiManager.showPiecePanel(*m_input.getSelectedPiece(), m_config);
    } else if (m_input.getSelectedBuilding()) {
        Building* bld = m_input.getSelectedBuilding();
        if (bld->type == BuildingType::Barracks && bld->owner == humanKingdomId()) {
            m_uiManager.showBarracksPanel(*bld, kingdom(humanKingdomId()), m_config);
        } else {
            m_uiManager.showBuildingPanel(*bld);
        }
    }
}
