#include "Core/Game.hpp"
#include "Render/OverlayRenderer.hpp"
#include "Save/SaveData.hpp"
#include "Buildings/BuildingType.hpp"
#include "Systems/BuildSystem.hpp"
#include "Input/InputContext.hpp"
#include "UI/InGameViewModelBuilder.hpp"
#include "Multiplayer/PasswordUtils.hpp"
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

void Game::configureLocalPlayerContext(const GameSessionConfig& session) {
    m_localPlayerContext = makeLocalPlayerContextForSession(session);
}

bool Game::isLocalPlayerTurn() const {
    return m_localPlayerContext.isLocallyControlled(turnSystem().getActiveKingdom());
}

bool Game::canLocalPlayerIssueCommands() const {
    return (m_state == GameState::Playing)
        && !m_waitingForRemoteTurnResult
        && isMultiplayerSessionReady()
        && isLocalPlayerTurn();
}

KingdomId Game::localPerspectiveKingdom() const {
    if (isLocalPlayerTurn()) {
        return turnSystem().getActiveKingdom();
    }

    return m_localPlayerContext.perspectiveKingdom;
}

bool Game::isMultiplayerSessionReady() const {
    if (isLanHost()) {
        return m_multiplayerServer.hasAuthenticatedClient();
    }

    if (isLanClient()) {
        return m_multiplayerClient.isAuthenticated();
    }

    return true;
}

GameMenuPresentation Game::buildGameMenuPresentation() const {
    GameMenuPresentation presentation;
    presentation.pauseState = m_localPlayerContext.isNetworked()
        ? GameMenuPauseState::NotPaused
        : GameMenuPauseState::Paused;
    presentation.showSave = !isLanClient();
    return presentation;
}

bool Game::isInGameMenuOpen() const {
    return m_uiManager.isGameMenuVisible();
}

void Game::openInGameMenu() {
    if (m_state != GameState::Playing && m_state != GameState::Paused) {
        return;
    }

    if (!m_localPlayerContext.isNetworked()) {
        m_state = GameState::Paused;
    }

    m_uiManager.showGameMenu(buildGameMenuPresentation());
}

void Game::closeInGameMenu() {
    m_uiManager.hideGameMenu();
    if (!m_localPlayerContext.isNetworked() && m_state == GameState::Paused) {
        m_state = GameState::Playing;
    }
}

void Game::toggleInGameMenu() {
    if (isInGameMenuOpen()) {
        closeInGameMenu();
    } else {
        openInGameMenu();
    }
}

void Game::updateMultiplayerPresentation() {
    if (!m_localPlayerContext.isNetworked()) {
        m_uiManager.hideMultiplayerWaitingOverlay();
        m_uiManager.clearMultiplayerStatus();
        return;
    }

    if (isLanHost()) {
        if (!m_multiplayerServer.hasAuthenticatedClient()) {
            std::string waitingMessage = "Share this endpoint with Black:\n";
            if (!m_multiplayerHostJoinHint.empty()) {
                waitingMessage += m_multiplayerHostJoinHint;
            } else {
                waitingMessage += "Port " + std::to_string(m_engine.sessionConfig().multiplayer.port);
            }
            waitingMessage += "\n\nWhite stays locked until the remote player finishes joining.";
            m_uiManager.showMultiplayerWaitingOverlay(
                "Waiting for Black Player",
                waitingMessage,
                "Return to Main Menu",
                [this]() {
                    returnToMainMenu();
                });
            m_uiManager.setMultiplayerStatus("LAN Host | Waiting for Black", MultiplayerStatusTone::Waiting);
            return;
        }

        m_uiManager.hideMultiplayerWaitingOverlay();
        if (turnSystem().getActiveKingdom() == KingdomId::Black) {
            m_uiManager.setMultiplayerStatus("LAN Host | Waiting for Black turn", MultiplayerStatusTone::Waiting);
        } else {
            m_uiManager.setMultiplayerStatus("LAN Host | Black connected", MultiplayerStatusTone::Connected);
        }
        return;
    }

    m_uiManager.hideMultiplayerWaitingOverlay();
    if (!m_multiplayerClient.isAuthenticated()) {
        m_uiManager.setMultiplayerStatus("LAN Client | Finalizing connection", MultiplayerStatusTone::Waiting);
        return;
    }

    if (m_waitingForRemoteTurnResult) {
        m_uiManager.setMultiplayerStatus("LAN Client | Waiting for host confirmation", MultiplayerStatusTone::Waiting);
    } else if (isLocalPlayerTurn()) {
        m_uiManager.setMultiplayerStatus("LAN Client | Connected - your turn", MultiplayerStatusTone::Connected);
    } else {
        m_uiManager.setMultiplayerStatus("LAN Client | Connected - White is playing", MultiplayerStatusTone::Neutral);
    }
}

void Game::returnToMainMenu() {
    stopMultiplayer();
    discardPendingAITurn();
    m_input.clearMovePreview();
    m_input.setTool(ToolState::Select);
    turnSystem().resetPendingCommands();
    m_state = GameState::MainMenu;
    m_uiManager.hideGameMenu();
    m_uiManager.showMainMenu();
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

void Game::centerCameraOnKingdom(KingdomId kingdom) {
    Piece* king = this->kingdom(kingdom).getKing();
    if (!king) {
        return;
    }

    const float centerX = static_cast<float>(king->position.x * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
    const float centerY = static_cast<float>(king->position.y * m_config.getCellSizePx() + m_config.getCellSizePx() / 2);
    m_camera.centerOn({centerX, centerY});
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

        if (m_uiManager.isMultiplayerAlertVisible() || m_uiManager.isMultiplayerWaitingOverlayVisible()) {
            continue;
        }

        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
            if (m_state != GameState::Playing && m_state != GameState::Paused) {
                continue;
            }

            toggleInGameMenu();
            continue;
        }

        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::P) {
            if (m_state == GameState::Playing || m_state == GameState::Paused || m_state == GameState::GameOver) {
                m_debugRecorder.exportHistory(gameName(),
                                             turnSystem().getTurnNumber(),
                                             turnSystem().getActiveKingdom());
            }
        }

        if (isInGameMenuOpen()) {
            continue;
        }

        // Spacebar acts as the Play button (commit turn)
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Space) {
            if (canLocalPlayerIssueCommands()) {
                commitPlayerTurn();
            }
        }

        if (m_state == GameState::Playing) {
            const bool allowCommands = canLocalPlayerIssueCommands();
            const KingdomId perspectiveKingdom = localPerspectiveKingdom();
            Kingdom& selectableKingdom = allowCommands ? activeKingdom() : kingdom(perspectiveKingdom);
            Kingdom& opposingKingdom = allowCommands ? enemyKingdom() : kingdom(opponent(perspectiveKingdom));
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
    if ((m_state == GameState::Playing || m_state == GameState::Paused || m_state == GameState::GameOver)
        && !m_uiManager.isMultiplayerAlertVisible()
        && !m_uiManager.isMultiplayerWaitingOverlayVisible()
        && !m_uiManager.isGameMenuVisible()) {
        m_input.updateCameraMovement(m_clock.getDeltaTime(), m_camera);
    }

    if (m_state == GameState::Playing || m_state == GameState::Paused || isLanClient() || isLanHost()) {
        updateMultiplayer();
    }

    switch (m_state) {
        case GameState::Playing: {
            if (m_engine.isActiveAI()) {
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

        const bool showActionOverlays = canLocalPlayerIssueCommands();
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
    stopMultiplayer();

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

    configureLocalPlayerContext(session);
    m_waitingForRemoteTurnResult = false;
    if (!startMultiplayerHostIfNeeded(session, errorMessage)) {
        stopMultiplayer();
        return false;
    }

    m_debugRecorder.reset();
    m_debugRecorder.recordSnapshot(turnSystem().getTurnNumber(),
                                   turnSystem().getActiveKingdom(),
                                   kingdoms(),
                                   "initial_state_new_game");

    // Center camera on the primary human side, or white when spectating AI vs AI.
    centerCameraOnKingdom(localPerspectiveKingdom());

    // Switch to playing
    m_state = GameState::Playing;
    refreshTurnPhase();
    m_uiManager.showHUD();
    updateUIState();
    saveGame();
    return true;
}

bool Game::loadGame(const std::string& saveName) {
    stopMultiplayer();
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
    configureLocalPlayerContext(m_engine.sessionConfig());
    m_waitingForRemoteTurnResult = false;
    if (!startMultiplayerHostIfNeeded(m_engine.sessionConfig(), &error)) {
        std::cerr << "Failed to start multiplayer host: " << error << std::endl;
        stopMultiplayer();
        return false;
    }

    m_debugRecorder.reset();
    m_debugRecorder.recordSnapshot(turnSystem().getTurnNumber(),
                                   turnSystem().getActiveKingdom(),
                                   kingdoms(),
                                   "initial_state_loaded_game");

    // Center camera on human player's king
    centerCameraOnKingdom(localPerspectiveKingdom());

    m_state = GameState::Playing;
    refreshTurnPhase();
    m_uiManager.showHUD();
    updateUIState();
    return true;
}

bool Game::saveGame() {
    if (isLanClient()) {
        std::cerr << "Client-side multiplayer sessions cannot save authoritative game state." << std::endl;
        return false;
    }

    SaveData data = m_engine.createSaveData();
    std::string path = "saves/" + gameName() + ".json";
    if (!m_saveManager.save(path, data)) {
        std::cerr << "Failed to save game!" << std::endl;
        return false;
    }

    return true;
}

void Game::commitPlayerTurn() {
    if (isLanClient()) {
        std::string error;
        if (!submitClientTurn(&error)) {
            std::cerr << "Failed to submit multiplayer turn: " << error << std::endl;
            if (!error.empty()) {
                m_uiManager.showMultiplayerAlert("Turn Not Sent", error);
            }
        }
        return;
    }

    commitAuthoritativeTurn();
}

void Game::commitAuthoritativeTurn() {
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
        if (isLanHost()) {
            saveGame();
            pushSnapshotToRemote(nullptr);
        }
        updateUIState();
        return;
    }

    turnSystem().advanceTurn();
    m_input.clearMovePreview();
    m_input.clearSelection();
    m_waitingForRemoteTurnResult = false;
    refreshTurnPhase();
    if (isLanHost()) {
        saveGame();
        pushSnapshotToRemote(nullptr);
    }
    startAITurnIfNeeded();
}

void Game::resetPlayerTurn() {
    m_input.cancelLiveMove();  // restore piece position if a live move is pending
    turnSystem().resetPendingCommands();
    m_input.clearSelection();
}

void Game::stopMultiplayer() {
    if (m_multiplayerServer.isRunning()) {
        m_multiplayerServer.sendDisconnectNotice("Host closed the multiplayer session.", nullptr);
    }

    m_multiplayerServer.stop();
    m_multiplayerClient.disconnect();
    m_waitingForRemoteTurnResult = false;
    m_multiplayerHostJoinHint.clear();
    m_localPlayerContext = LocalPlayerContext{};
    m_uiManager.hideMultiplayerWaitingOverlay();
    m_uiManager.hideMultiplayerAlert();
    m_uiManager.clearMultiplayerStatus();
}

bool Game::startMultiplayerHostIfNeeded(const GameSessionConfig& session, std::string* errorMessage) {
    if (!session.multiplayer.enabled) {
        return true;
    }

    if (!m_multiplayerServer.start(static_cast<unsigned short>(session.multiplayer.port),
                                   session.saveName,
                                   session.multiplayer,
                                   errorMessage)) {
        return false;
    }

    const sf::IpAddress localAddress = sf::IpAddress::getLocalAddress();
    if (localAddress == sf::IpAddress::None) {
        m_multiplayerHostJoinHint = "LAN port " + std::to_string(session.multiplayer.port);
    } else {
        m_multiplayerHostJoinHint = localAddress.toString() + ":" + std::to_string(session.multiplayer.port);
    }

    eventLog().log(turnSystem().getTurnNumber(), KingdomId::White,
                   "LAN server started on port " + std::to_string(session.multiplayer.port) + ".");
    return true;
}

bool Game::pushSnapshotToRemote(std::string* errorMessage) {
    if (!isLanHost() || !m_multiplayerServer.hasAuthenticatedClient()) {
        return true;
    }

    const std::string snapshot = m_saveManager.serialize(m_engine.createSaveData());
    return m_multiplayerServer.sendSnapshot(snapshot, errorMessage);
}

bool Game::submitClientTurn(std::string* errorMessage) {
    if (!isLanClient()) {
        return false;
    }
    if (!m_multiplayerClient.isAuthenticated()) {
        if (errorMessage) {
            *errorMessage = "The multiplayer host connection is not authenticated.";
        }
        return false;
    }

    if (!m_multiplayerClient.sendTurnSubmission(turnSystem().getPendingCommands(), errorMessage)) {
        return false;
    }

    m_waitingForRemoteTurnResult = true;
    m_input.clearMovePreview();
    m_input.clearSelection();
    turnSystem().resetPendingCommands();
    return true;
}

bool Game::applyRemoteTurnSubmission(const std::vector<TurnCommand>& commands, std::string* errorMessage) {
    if (!isLanHost()) {
        if (errorMessage) {
            *errorMessage = "Cannot apply a remote turn submission outside LAN host mode.";
        }
        return false;
    }

    if (turnSystem().getActiveKingdom() != KingdomId::Black) {
        if (errorMessage) {
            *errorMessage = "Remote turns are only accepted when Black is the active kingdom.";
        }
        return false;
    }

    turnSystem().resetPendingCommands();
    for (const auto& command : commands) {
        if (!turnSystem().queueCommand(command)) {
            turnSystem().resetPendingCommands();
            if (errorMessage) {
                *errorMessage = "The remote player submitted an invalid or conflicting turn command.";
            }
            return false;
        }
    }

    commitAuthoritativeTurn();
    return true;
}

bool Game::joinMultiplayer(const JoinMultiplayerRequest& request, std::string* errorMessage) {
    stopMultiplayer();
    discardPendingAITurn();
    m_input.clearMovePreview();
    m_input.setTool(ToolState::Select);
    m_engine.resetPendingTurn();

    const sf::IpAddress address(request.host);
    if (address == sf::IpAddress::None) {
        if (errorMessage) {
            *errorMessage = "Invalid server IP address.";
        }
        return false;
    }

    if (!m_multiplayerClient.connect(address,
                                     static_cast<unsigned short>(request.port),
                                     sf::seconds(3.f),
                                     errorMessage)) {
        return false;
    }
    if (!m_multiplayerClient.requestServerInfo(errorMessage)) {
        m_multiplayerClient.disconnect();
        return false;
    }

    MultiplayerServerInfo serverInfo;
    bool hasServerInfo = false;
    sf::Clock clock;
    while (clock.getElapsedTime() < sf::seconds(3.f) && !hasServerInfo) {
        m_multiplayerClient.update();
        while (m_multiplayerClient.hasPendingEvent()) {
            const auto event = m_multiplayerClient.popNextEvent();
            if (event.type == MultiplayerClient::Event::Type::ServerInfoReceived) {
                serverInfo = event.serverInfo;
                hasServerInfo = true;
                break;
            }
            if (event.type == MultiplayerClient::Event::Type::Disconnected
                || event.type == MultiplayerClient::Event::Type::Error) {
                if (errorMessage) {
                    *errorMessage = event.message.empty() ? "The multiplayer host did not respond." : event.message;
                }
                m_multiplayerClient.disconnect();
                return false;
            }
        }
    }

    if (!hasServerInfo) {
        if (errorMessage) {
            *errorMessage = "The multiplayer host did not answer the ping request.";
        }
        m_multiplayerClient.disconnect();
        return false;
    }
    if (!serverInfo.multiplayerEnabled) {
        if (errorMessage) {
            *errorMessage = "This server is not hosting a multiplayer save.";
        }
        m_multiplayerClient.disconnect();
        return false;
    }
    if (serverInfo.protocolVersion != kCurrentMultiplayerProtocolVersion) {
        if (errorMessage) {
            *errorMessage = "The multiplayer host uses an incompatible protocol version.";
        }
        m_multiplayerClient.disconnect();
        return false;
    }
    if (!serverInfo.joinable) {
        if (errorMessage) {
            *errorMessage = "The multiplayer server is already occupied.";
        }
        m_multiplayerClient.disconnect();
        return false;
    }

    const std::string digest = MultiplayerPasswordUtils::computePasswordDigest(
        request.password, serverInfo.passwordSalt);
    if (!m_multiplayerClient.sendJoinRequest(digest, errorMessage)) {
        m_multiplayerClient.disconnect();
        return false;
    }

    bool accepted = false;
    bool receivedSnapshot = false;
    SaveData snapshotData;
    clock.restart();
    while (clock.getElapsedTime() < sf::seconds(5.f) && !receivedSnapshot) {
        m_multiplayerClient.update();
        while (m_multiplayerClient.hasPendingEvent()) {
            const auto event = m_multiplayerClient.popNextEvent();
            if (event.type == MultiplayerClient::Event::Type::JoinRejected) {
                if (errorMessage) {
                    *errorMessage = event.message.empty() ? "The multiplayer host rejected the join request." : event.message;
                }
                m_multiplayerClient.disconnect();
                return false;
            }
            if (event.type == MultiplayerClient::Event::Type::JoinAccepted) {
                accepted = true;
                continue;
            }
            if (event.type == MultiplayerClient::Event::Type::SnapshotReceived) {
                if (!m_saveManager.deserialize(event.serializedSaveData, snapshotData)) {
                    if (errorMessage) {
                        *errorMessage = "The multiplayer host sent an invalid game snapshot.";
                    }
                    m_multiplayerClient.disconnect();
                    return false;
                }
                receivedSnapshot = true;
                break;
            }
            if (event.type == MultiplayerClient::Event::Type::Disconnected
                || event.type == MultiplayerClient::Event::Type::Error) {
                if (errorMessage) {
                    *errorMessage = event.message.empty() ? "Lost connection to the multiplayer host." : event.message;
                }
                m_multiplayerClient.disconnect();
                return false;
            }
        }
    }

    if (!accepted || !receivedSnapshot) {
        if (errorMessage) {
            *errorMessage = "The multiplayer host did not complete the join handshake in time.";
        }
        m_multiplayerClient.disconnect();
        return false;
    }

    std::string restoreError;
    if (!m_engine.restoreFromSave(snapshotData, m_config, &restoreError)) {
        if (errorMessage) {
            *errorMessage = restoreError.empty() ? "Unable to restore the multiplayer snapshot." : restoreError;
        }
        m_multiplayerClient.disconnect();
        return false;
    }

    m_localPlayerContext = makeLanClientLocalPlayerContext();
    m_waitingForRemoteTurnResult = false;
    m_state = GameState::Playing;
    refreshTurnPhase();
    centerCameraOnKingdom(KingdomId::Black);
    m_uiManager.showHUD();
    updateUIState();
    return true;
}

void Game::updateMultiplayer() {
    if (isLanHost()) {
        m_multiplayerServer.update();
        while (m_multiplayerServer.hasPendingEvent()) {
            processMultiplayerServerEvent(m_multiplayerServer.popNextEvent());
        }
    }

    if (isLanClient()) {
        m_multiplayerClient.update();
        while (m_multiplayerClient.hasPendingEvent()) {
            processMultiplayerClientEvent(m_multiplayerClient.popNextEvent());
        }
    }
}

void Game::processMultiplayerServerEvent(const MultiplayerServer::Event& event) {
    switch (event.type) {
        case MultiplayerServer::Event::Type::ClientConnected:
            m_uiManager.hideMultiplayerAlert();
            eventLog().log(turnSystem().getTurnNumber(), KingdomId::Black, event.message);
            pushSnapshotToRemote(nullptr);
            break;

        case MultiplayerServer::Event::Type::ClientDisconnected:
            eventLog().log(turnSystem().getTurnNumber(), KingdomId::Black, event.message);
            m_uiManager.showMultiplayerAlert(
                "Player Disconnected",
                event.message + "\n\nWhite is locked until Black reconnects.",
                "Continue");
            break;

        case MultiplayerServer::Event::Type::TurnSubmitted: {
            std::string error;
            if (!applyRemoteTurnSubmission(event.commands, &error)) {
                m_multiplayerServer.sendTurnRejected(
                    error.empty() ? "The host rejected the submitted turn." : error,
                    nullptr);
                eventLog().log(turnSystem().getTurnNumber(), KingdomId::Black,
                               error.empty() ? "Rejected remote multiplayer turn." : error);
                pushSnapshotToRemote(nullptr);
            }
            break;
        }

        case MultiplayerServer::Event::Type::Error:
            eventLog().log(turnSystem().getTurnNumber(), KingdomId::Black, event.message);
            m_uiManager.showMultiplayerAlert(
                "LAN Host Error",
                event.message.empty() ? "The LAN host encountered a network error." : event.message,
                "Continue");
            break;
    }
}

void Game::processMultiplayerClientEvent(const MultiplayerClient::Event& event) {
    switch (event.type) {
        case MultiplayerClient::Event::Type::SnapshotReceived: {
            SaveData snapshotData;
            if (!m_saveManager.deserialize(event.serializedSaveData, snapshotData)) {
                std::cerr << "Received an invalid multiplayer snapshot from the host." << std::endl;
                return;
            }

            std::string restoreError;
            if (!m_engine.restoreFromSave(snapshotData, m_config, &restoreError)) {
                std::cerr << "Failed to restore multiplayer snapshot: " << restoreError << std::endl;
                return;
            }

            m_localPlayerContext = makeLanClientLocalPlayerContext();
            m_waitingForRemoteTurnResult = false;
            m_input.clearMovePreview();
            m_input.clearSelection();
            refreshTurnPhase();
            updateUIState();
            break;
        }

        case MultiplayerClient::Event::Type::TurnRejected:
            m_uiManager.showMultiplayerAlert(
                "Turn Rejected",
                (event.message.empty() ? std::string{"The host rejected the submitted turn."} : event.message)
                    + "\n\nYour client stays synchronized with the host snapshot.",
                "Continue");
            break;

        case MultiplayerClient::Event::Type::Disconnected:
        case MultiplayerClient::Event::Type::Error: {
            const std::string message = event.message.empty()
                ? (event.type == MultiplayerClient::Event::Type::Disconnected
                    ? "Multiplayer host disconnected."
                    : "The multiplayer client encountered a network error.")
                : event.message;
            std::cerr << message << std::endl;
            m_uiManager.showMultiplayerAlert(
                event.type == MultiplayerClient::Event::Type::Disconnected ? "Host Disconnected" : "Network Error",
                message,
                "Return to Main Menu",
                [this]() {
                    returnToMainMenu();
                });
            break;
        }

        default:
            break;
    }
}

void Game::setupUICallbacks() {
    // Game menu
    m_uiManager.gameMenu().setOnResume([this]() {
        closeInGameMenu();
    });
    m_uiManager.gameMenu().setOnSave([this]() {
        saveGame();
    });
    m_uiManager.gameMenu().setOnQuitToMainMenu([this]() {
        returnToMainMenu();
    });

    // Main menu
    m_uiManager.mainMenu().setOnLoadSaves([this]() {
        m_uiManager.mainMenu().setSaves(m_saveManager.listSaveSummaries("saves"));
    });
    m_uiManager.mainMenu().setOnExitGame([this]() {
        stopMultiplayer();
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
    m_uiManager.mainMenu().setOnJoinMultiplayer([this](const JoinMultiplayerRequest& request) {
        std::string error;
        if (!joinMultiplayer(request, &error)) {
            return error;
        }
        return std::string{};
    });
    m_uiManager.mainMenu().setOnDeleteSave([this](const std::string& saveName) {
        if (saveName.empty()) {
            return;
        }

        m_saveManager.deleteSave("saves/" + saveName + ".json");
        m_uiManager.mainMenu().setSaves(m_saveManager.listSaveSummaries("saves"));
    });

    // HUD
    m_uiManager.hud().setOnMenu([this]() {
        if (m_state != GameState::Playing && m_state != GameState::Paused) {
            return;
        }

        toggleInGameMenu();
    });
    m_uiManager.hud().setOnResetTurn([this]() {
        if (!canLocalPlayerIssueCommands()) return;
        resetPlayerTurn();
    });
    m_uiManager.hud().setOnEndTurn([this]() {
        if (!canLocalPlayerIssueCommands()) return;
        commitPlayerTurn();
    });

    // Toolbar
    m_uiManager.toolBar().setOnSelect([this]() {
        m_input.setTool(ToolState::Select);
        m_uiManager.showSelectionEmptyState();
    });
    m_uiManager.toolBar().setOnBuild([this]() {
        if (!canLocalPlayerIssueCommands()) return;
        m_input.setTool(ToolState::Build);
        m_uiManager.showBuildToolPanel(activeKingdom(), m_config, true);
    });

    // Build tool panel
    m_uiManager.buildToolPanel().setOnSelectBuildType([this](int type) {
        if (!canLocalPlayerIssueCommands()) return;
        m_input.setBuildType(static_cast<BuildingType>(type));
    });

    // Piece panel upgrade
    m_uiManager.piecePanel().setOnUpgrade([this](int pieceId, int targetType) {
        if (!canLocalPlayerIssueCommands()) return;
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
        if (!canLocalPlayerIssueCommands()) return;
        TurnCommand cmd;
        cmd.type = TurnCommand::Produce;
        cmd.barracksId = barracksId;
        cmd.produceType = static_cast<PieceType>(pieceType);
        turnSystem().queueCommand(cmd);
    });
}

void Game::updateUIState() {
    const bool allowCommands = canLocalPlayerIssueCommands();
    const InGameViewModel viewModel = buildInGameViewModel(m_engine, m_config, m_state, allowCommands);
    m_uiManager.updateDashboard(viewModel);
    updateMultiplayerPresentation();
    const KingdomId viewedKingdomId = allowCommands ? activeKingdom().id : localPerspectiveKingdom();

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
    if (m_state != GameState::Playing || !m_engine.isActiveAI()) {
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
