#include "Core/Game.hpp"
#include "Render/OverlayRenderer.hpp"
#include "Save/SaveData.hpp"
#include "Buildings/BuildingType.hpp"
#include "Systems/BuildSystem.hpp"
#include "Input/InputContext.hpp"
#include "UI/InGameViewModelBuilder.hpp"
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

AIWorldSnapshot makeAIWorldSnapshot(const Board& board,
                                    const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                    const std::vector<Building>& publicBuildings) {
    AIWorldSnapshot snapshot{board, kingdoms, publicBuildings};
    relinkBoardState(snapshot.board, snapshot.kingdoms, snapshot.publicBuildings);
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
    : m_state(GameState::MainMenu) {}

KingdomId Game::humanKingdomId() const {
    return m_engine.humanKingdomId();
}

std::string Game::participantName(KingdomId id) const {
    return m_engine.participantName(id);
}

std::string Game::activeTurnLabel() const {
    return m_engine.activeTurnLabel();
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
                m_debugRecorder.exportHistory(gameName(),
                                             turnSystem().getTurnNumber(),
                                             turnSystem().getActiveKingdom());
            }
        }

        // Spacebar acts as the Play button (commit turn)
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Space) {
            if (m_state == GameState::Playing && isActiveHuman()) {
                commitPlayerTurn();
            }
        }

        if (m_state == GameState::Playing) {
            const bool allowCommands = isActiveHuman();
            Kingdom& selectableKingdom = allowCommands ? activeKingdom() : kingdom(humanKingdomId());
            Kingdom& opposingKingdom = allowCommands ? enemyKingdom() : kingdom(opponent(humanKingdomId()));
            const InputContext inputContext{
                m_window,
                m_camera,
                board(),
                turnSystem(),
                selectableKingdom,
                opposingKingdom,
                publicBuildings(),
                m_uiManager,
                m_config,
                allowCommands
            };
            m_input.handleEvent(event, inputContext);
        }
    }
}

void Game::update() {
    switch (m_state) {
        case GameState::Playing: {
            if (isActiveAI()) {
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
        m_renderer.drawWorldBase(m_window, m_camera, board(), kingdoms(),
            publicBuildings());

        const bool showActionOverlays = isActiveHuman();
        const Piece* selectedPiece = m_input.getSelectedPiece();
        const bool canShowSelectedPieceActions = showActionOverlays
            && selectedPiece
            && selectedPiece->kingdom == activeKingdom().id;

        if (canShowSelectedPieceActions && m_input.getCurrentTool() == ToolState::Select) {
            sf::Vector2i highlightedOrigin = m_input.hasMovePreview()
                ? m_input.getMoveFrom()
                : selectedPiece->position;
            m_renderer.getOverlay().drawOriginCell(m_window, m_camera,
                highlightedOrigin, m_config.getCellSizePx());
            m_renderer.getOverlay().drawReachableCells(m_window, m_camera,
                m_input.getValidMoves(), m_config.getCellSizePx());
            if (!m_input.getDangerMoves().empty()) {
                m_renderer.getOverlay().drawDangerCells(m_window, m_camera,
                    m_input.getDangerMoves(), m_config.getCellSizePx());
            }
        }

        m_renderer.drawPiecesLayer(m_window, m_camera, kingdoms());

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
            } else if (m_input.hasSelectedCell()) {
                m_renderer.getOverlay().drawSelectionFrame(m_window, m_camera,
                    m_hudView, m_windowSize, m_input.getSelectedCell(),
                    1, 1, m_config.getCellSizePx());
            }
        }
        if (showActionOverlays && m_input.hasBuildPreview()) {
            BuildingType bt = m_input.getBuildPreviewType();
            int bw = m_config.getBuildingWidth(bt);
            int bh = m_config.getBuildingHeight(bt);
            Kingdom& builder = activeKingdom();
            Piece* king = builder.getKing();
            bool valid = king && BuildSystem::canBuild(bt, m_input.getBuildPreviewOrigin(),
                                                        *king, board(), builder, m_config);
            m_renderer.getOverlay().drawBuildPreview(m_window, m_camera,
                m_input.getBuildPreviewOrigin(), bw, bh, m_config.getCellSizePx(), valid);
        }
        if (showActionOverlays) {
            if (const TurnCommand* pendingBuild = turnSystem().getPendingBuildCommand()) {
            int bw = m_config.getBuildingWidth(pendingBuild->buildingType);
            int bh = m_config.getBuildingHeight(pendingBuild->buildingType);
            m_renderer.getOverlay().drawBuildPreview(m_window, m_camera,
                pendingBuild->buildOrigin, bw, bh, m_config.getCellSizePx(), true);
            }
        }
        m_renderer.getOverlay().drawZoneIndicators(m_window, m_camera, m_hudView,
            m_windowSize, board(),
            publicBuildings(), kingdoms(), m_config.getCellSizePx(), m_assets);
    }

    m_window.setView(m_hudView);
    m_gui.draw();
    m_window.display();
}

bool Game::startNewGame(const GameSessionConfig& session, std::string* errorMessage) {
    const auto existingSaves = m_saveManager.listSaves("saves");
    if (std::find(existingSaves.begin(), existingSaves.end(), session.saveName) != existingSaves.end()) {
        if (errorMessage) {
            *errorMessage = "A save with this name already exists.";
        }
        return false;
    }

    discardPendingAITurn();
    m_input.clearMovePreview();
    m_input.setTool(ToolState::Select);
    m_engine.resetPendingTurn();

    if (!m_engine.startNewSession(session, m_config, errorMessage)) {
        return false;
    }

    m_debugRecorder.reset();
    m_debugRecorder.recordSnapshot(turnSystem().getTurnNumber(),
                                   turnSystem().getActiveKingdom(),
                                   kingdoms(),
                                   "initial_state_new_game");

    // Center camera on the primary human side, or white when spectating AI vs AI.
    Piece* focusKing = kingdom(humanKingdomId()).getKing();
    if (focusKing) {
        float cx = static_cast<float>(focusKing->position.x * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
        float cy = static_cast<float>(focusKing->position.y * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
        m_camera.centerOn({cx, cy});
    }

    // Switch to playing
    m_state = GameState::Playing;
    refreshTurnPhase();
    m_uiManager.showHUD();
    updateUIState();
    saveGame();
    return true;
}

bool Game::loadGame(const std::string& saveName) {
    discardPendingAITurn();
    m_input.clearMovePreview();
    m_input.setTool(ToolState::Select);
    m_engine.resetPendingTurn();

    SaveData data;
    std::string path = "saves/" + saveName + ".json";
    if (!m_saveManager.load(path, data)) {
        std::cerr << "Failed to load save: " << path << std::endl;
        return false;
    }

    if (data.gameName.empty()) {
        data.gameName = saveName;
    }

    std::string error;
    if (!m_engine.restoreFromSave(data, m_config, &error)) {
        std::cerr << "Failed to restore save: " << error << std::endl;
        return false;
    }
    m_debugRecorder.reset();
    m_debugRecorder.recordSnapshot(turnSystem().getTurnNumber(),
                                   turnSystem().getActiveKingdom(),
                                   kingdoms(),
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
    updateUIState();
    return true;
}

bool Game::saveGame() {
    SaveData data = m_engine.createSaveData();
    std::string path = "saves/" + gameName() + ".json";
    if (!m_saveManager.save(path, data)) {
        std::cerr << "Failed to save game!" << std::endl;
        return false;
    }

    return true;
}

void Game::commitPlayerTurn() {
    KingdomId activeId = turnSystem().getActiveKingdom();
    KingdomId enemyId  = opponent(activeId);

    turnSystem().commitTurn(board(), activeKingdom(), enemyKingdom(),
                            publicBuildings(), m_config, eventLog(),
                            pieceFactory(), buildingFactory());
    m_debugRecorder.logTurnState(turnSystem().getTurnNumber(), kingdoms(), "after player commit");
    m_debugRecorder.recordSnapshot(turnSystem().getTurnNumber(),
                                   turnSystem().getActiveKingdom(),
                                   kingdoms(),
                                   "after_player_commit");

    // Check if opponent is in checkmate
    if (CheckSystem::isCheckmate(enemyId, board(), m_config)) {
        m_state = GameState::GameOver;
        std::string winner = participantName(activeId);
        eventLog().log(turnSystem().getTurnNumber(), activeId,
                       "Checkmate! " + winner + " wins!");
        m_input.clearMovePreview();
        m_input.clearSelection();
        updateUIState();
        return;
    }

    turnSystem().advanceTurn();
    m_input.clearMovePreview();
    m_input.clearSelection();
    refreshTurnPhase();
    startAITurnIfNeeded();
}

void Game::resetPlayerTurn() {
    m_input.cancelLiveMove();  // restore piece position if a live move is pending
    turnSystem().resetPendingCommands();
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
        m_input.clearMovePreview();
        m_input.setTool(ToolState::Select);
        turnSystem().resetPendingCommands();
        m_state = GameState::MainMenu;
        m_uiManager.hidePauseMenu();
        m_uiManager.showMainMenu();
    });

    // Main menu
    m_uiManager.mainMenu().setOnLoadSaves([this]() {
        m_uiManager.mainMenu().setSaves(m_saveManager.listSaveSummaries("saves"));
    });
    m_uiManager.mainMenu().setOnExitGame([this]() {
        m_window.close();
    });
    m_uiManager.mainMenu().setOnCreateSave([this](const GameSessionConfig& session) {
        std::string error;
        if (!startNewGame(session, &error)) {
            return error;
        }
        return std::string{};
    });
    m_uiManager.mainMenu().setOnPlaySave([this](const std::string& saveName) {
        loadGame(saveName);
    });
    m_uiManager.mainMenu().setOnDeleteSave([this](const std::string& saveName) {
        if (saveName.empty()) {
            return;
        }

        m_saveManager.deleteSave("saves/" + saveName + ".json");
        m_uiManager.mainMenu().setSaves(m_saveManager.listSaveSummaries("saves"));
    });

    // HUD
    m_uiManager.hud().setOnPause([this]() {
        if (m_state == GameState::Playing) {
            m_state = GameState::Paused;
            m_uiManager.showPauseMenu();
        } else if (m_state == GameState::Paused) {
            m_state = GameState::Playing;
            m_uiManager.hidePauseMenu();
        }
    });
    m_uiManager.hud().setOnResetTurn([this]() {
        if (!isActiveHuman()) return;
        resetPlayerTurn();
    });
    m_uiManager.hud().setOnEndTurn([this]() {
        if (!isActiveHuman()) return;
        commitPlayerTurn();
    });

    // Toolbar
    m_uiManager.toolBar().setOnSelect([this]() {
        m_input.setTool(ToolState::Select);
        m_uiManager.showSelectionEmptyState();
    });
    m_uiManager.toolBar().setOnBuild([this]() {
        if (!isActiveHuman()) return;
        m_input.setTool(ToolState::Build);
        m_uiManager.showBuildToolPanel(activeKingdom(), m_config, true);
    });

    // Build tool panel
    m_uiManager.buildToolPanel().setOnSelectBuildType([this](int type) {
        if (!isActiveHuman()) return;
        m_input.setBuildType(static_cast<BuildingType>(type));
    });

    // Piece panel upgrade
    m_uiManager.piecePanel().setOnUpgrade([this](int pieceId, int targetType) {
        if (!isActiveHuman()) return;
        Piece* piece = activeKingdom().getPieceById(pieceId);
        if (!piece) return;
        TurnCommand cmd;
        cmd.type = TurnCommand::Upgrade;
        cmd.upgradePieceId = piece->id;
        cmd.upgradeTarget = static_cast<PieceType>(targetType);
        if (!piece->canUpgradeTo(cmd.upgradeTarget, m_config)) {
            return;
        }
        turnSystem().queueCommand(cmd);
    });

    // Barracks panel produce
    m_uiManager.barracksPanel().setOnProduce([this](int barracksId, int pieceType) {
        if (!isActiveHuman()) return;
        TurnCommand cmd;
        cmd.type = TurnCommand::Produce;
        cmd.barracksId = barracksId;
        cmd.produceType = static_cast<PieceType>(pieceType);
        turnSystem().queueCommand(cmd);
    });
}

void Game::updateUIState() {
    const bool allowCommands = (m_state == GameState::Playing) && isActiveHuman();
    const InGameViewModel viewModel = buildInGameViewModel(m_engine, m_config, m_state, allowCommands);
    m_uiManager.updateDashboard(viewModel);
    const KingdomId viewedKingdomId = allowCommands ? activeKingdom().id : humanKingdomId();

    switch (m_input.getCurrentTool()) {
        case ToolState::Build:
            if (allowCommands) {
                m_uiManager.showBuildToolPanel(activeKingdom(), m_config, true);
            } else {
                m_uiManager.showSelectionEmptyState();
            }
            break;

        case ToolState::Select:
        default:
            if (m_input.getSelectedPiece()) {
                const bool allowUpgrade = allowCommands
                    && (m_input.getSelectedPiece()->kingdom == viewedKingdomId);
                m_uiManager.showPiecePanel(*m_input.getSelectedPiece(), m_config, allowUpgrade);
            } else if (m_input.getSelectedBuilding()) {
                Building* building = m_input.getSelectedBuilding();
                if (building->type == BuildingType::Barracks) {
                    const bool allowProduce = allowCommands && (building->owner == viewedKingdomId);
                    m_uiManager.showBarracksPanel(*building, kingdom(building->owner), m_config, allowProduce);
                } else {
                    m_uiManager.showBuildingPanel(*building);
                }
            } else if (m_input.hasSelectedCell()) {
                const sf::Vector2i cellPos = m_input.getSelectedCell();
                m_uiManager.showCellPanel(board().getCell(cellPos.x, cellPos.y));
            } else {
                m_uiManager.showSelectionEmptyState();
            }
            break;
    }
}

void Game::refreshTurnPhase() {
    m_turnPhase = (turnSystem().getActiveKingdom() == KingdomId::White)
        ? TurnPhase::WhiteTurn
        : TurnPhase::BlackTurn;
}

void Game::discardPendingAITurn() {
    m_aiTaskGeneration.fetch_add(1, std::memory_order_relaxed);
    m_aiTask.reset();
}

void Game::startAITurnIfNeeded() {
    if (m_state != GameState::Playing || !isActiveAI()) {
        return;
    }
    if (m_aiTask) {
        return;
    }

    const KingdomId activeId = turnSystem().getActiveKingdom();
    const int turnNumber = turnSystem().getTurnNumber();
    const std::uint64_t generation = m_aiTaskGeneration.load(std::memory_order_relaxed);

    auto task = std::make_shared<AsyncAITaskState>();
    task->generation = generation;
    task->activeKingdom = activeId;
    task->turnNumber = turnNumber;
    m_aiTask = task;

    AIWorldSnapshot snapshot = makeAIWorldSnapshot(board(), kingdoms(), publicBuildings());
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
        || turnSystem().getActiveKingdom() != task->activeKingdom
        || turnSystem().getTurnNumber() != task->turnNumber) {
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
        eventLog().log(task->turnNumber, task->activeKingdom, "AI Objective: " + directorPlan.objectiveName);
        for (const auto& cmd : directorPlan.commands) {
            turnSystem().queueCommand(cmd);
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
        eventLog().log(task->turnNumber, task->activeKingdom, "AI Phase: " + plan.phaseName);
        for (const auto& cmd : plan.commands) {
            turnSystem().queueCommand(cmd);
        }
    }
    eventLog().log(task->turnNumber, task->activeKingdom, "AI completed turn planning.");

    const KingdomId activeId = turnSystem().getActiveKingdom();
    const KingdomId enemyId = opponent(activeId);
    turnSystem().commitTurn(board(), activeKingdom(), enemyKingdom(),
        publicBuildings(), m_config, eventLog(),
        pieceFactory(), buildingFactory());
    m_debugRecorder.logTurnState(turnSystem().getTurnNumber(), kingdoms(), "after ai commit");
    m_debugRecorder.recordSnapshot(turnSystem().getTurnNumber(),
                                   turnSystem().getActiveKingdom(),
                                   kingdoms(),
                                   "after_ai_commit");

    if (CheckSystem::isCheckmate(enemyId, board(), m_config)) {
        m_state = GameState::GameOver;
        std::string winner = participantName(activeId);
        eventLog().log(turnSystem().getTurnNumber(), activeId,
            "Checkmate! " + winner + " wins!");
        updateUIState();
        return;
    }

    turnSystem().advanceTurn();
    refreshTurnPhase();
}
