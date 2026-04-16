#include "Core/Game.hpp"
#include "Save/SaveData.hpp"
#include "Buildings/BuildingType.hpp"
#include "Systems/BuildOverlayRules.hpp"
#include "Systems/BuildSystem.hpp"
#include "Systems/PendingTurnProjection.hpp"
#include "Runtime/InputCoordinator.hpp"
#include "Runtime/RenderCoordinator.hpp"
#include "Runtime/UpdateCoordinator.hpp"
#include "Input/InputContext.hpp"
#include "Multiplayer/PasswordUtils.hpp"
#include <algorithm>
#include <iostream>
#include <optional>

namespace {

}

#ifdef _WIN32
namespace {
Game* s_windowProcGame = nullptr;
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
    : m_state(GameState::MainMenu)
    , m_aiTurnCoordinator(m_engine, m_aiTurnRunner, m_aiDirector, m_config)
    , m_sessionFlow(m_engine, m_saveManager, m_multiplayer, m_debugRecorder, m_config)
    , m_multiplayerJoinCoordinator(m_engine, m_multiplayer, m_saveManager, m_input, m_config)
    , m_turnCoordinator(m_engine, m_multiplayer, m_debugRecorder, m_config)
    , m_sessionRuntimeCoordinator(
          m_state,
          m_waitingForRemoteTurnResult,
          m_localPlayerContext,
          m_engine,
          m_multiplayer,
          m_sessionFlow,
          m_multiplayerJoinCoordinator)
    , m_turnLifecycleCoordinator(
          m_state,
          m_waitingForRemoteTurnResult,
          m_engine,
          m_turnCoordinator,
          m_input,
          m_uiManager,
          m_aiTurnCoordinator)
    , m_panelActionCoordinator(m_engine, m_input, m_config)
    , m_multiplayerRuntimeCoordinator(
          m_multiplayer,
          m_saveManager,
          m_engine,
          m_input,
          m_uiManager,
          m_localPlayerContext,
          m_waitingForRemoteTurnResult,
          m_config) {}

void Game::configureLocalPlayerContext(const GameSessionConfig& session) {
    m_localPlayerContext = makeLocalPlayerContextForSession(session);
}

FrontendRuntimeState Game::makeFrontendRuntimeState() const {
    FrontendRuntimeState state;
    state.gameState = m_state;
    state.localPlayerContext = m_localPlayerContext;
    state.overlaysVisible = m_uiManager.isMultiplayerAlertVisible()
        || m_uiManager.isMultiplayerWaitingOverlayVisible();
    state.inGameMenuOpen = isInGameMenuOpen();
    state.waitingForRemoteTurnResult = m_waitingForRemoteTurnResult;
    state.hostAuthenticated = m_multiplayer.hostHasAuthenticatedClient();
    state.clientAuthenticated = m_multiplayer.clientIsAuthenticated();
    state.awaitingReconnect = m_multiplayer.awaitingReconnect();
    state.hasReconnectRequest = m_multiplayer.hasReconnectRequest();
    state.hostJoinHint = m_multiplayer.hostJoinHint();
    state.activeKingdom = turnSystem().getActiveKingdom();
    return state;
}

bool Game::isLocalPlayerTurn() const {
    return FrontendCoordinator::isLocalPlayerTurn(makeFrontendRuntimeState());
}

bool Game::canLocalPlayerIssueCommands() const {
    return currentInteractionPermissions().canIssueCommands;
}

UICallbackRuntimeState Game::makeUICallbackRuntimeState() const {
    UICallbackRuntimeState state;
    state.gameState = m_state;
    state.permissions = currentInteractionPermissions();
    return state;
}

UICallbackCoordinatorDependencies Game::makeUICallbackCoordinatorDependencies() {
    return UICallbackCoordinatorDependencies{
        [this]() {
            return makeUICallbackRuntimeState();
        },
        m_uiManager,
        m_saveManager,
        m_input,
        m_panelActionCoordinator,
        m_config,
        [this]() {
            closeInGameMenu();
        },
        [this]() {
            saveGame();
        },
        [this]() {
            returnToMainMenu();
        },
        [this](const GameSessionConfig& session, std::string* errorMessage) {
            return startNewGame(session, errorMessage);
        },
        [this](const std::string& saveName) {
            loadGame(saveName);
        },
        [this](const JoinMultiplayerRequest& request, std::string* errorMessage) {
            return joinMultiplayer(request, errorMessage);
        },
        [this]() {
            stopMultiplayer();
        },
        [this]() {
            m_window.close();
        },
        [this]() {
            toggleInGameMenu();
        },
        [this]() {
            resetPlayerTurn();
        },
        [this]() {
            commitPlayerTurn();
        },
        [this]() {
            activateSelectTool();
        },
        [this]() {
            m_uiManager.toggleRightSidebar();
        },
        [this]() {
            ensureTurnDraftUpToDate();
        },
        [this]() {
            return activeKingdom().id;
        },
        [this]() {
            return localPerspectiveKingdom();
        },
        [this](KingdomId id) -> Kingdom& {
            return displayedKingdom(id);
        }};
}

SessionRuntimeCallbacks Game::makeSessionRuntimeCallbacks() {
    return SessionRuntimeCallbacks{
        [this]() {
            stopMultiplayer();
        },
        [this]() {
            discardPendingAITurn();
        },
        [this]() {
            m_input.clearMovePreview();
        },
        [this]() {
            m_input.setTool(ToolState::Select);
        },
        [this]() {
            m_engine.resetPendingTurn();
        },
        [this]() {
            turnSystem().resetPendingCommands();
        },
        [this]() {
            invalidateTurnDraft();
        },
        [this](KingdomId kingdom) {
            centerCameraOnKingdom(kingdom);
        },
        [this]() {
            refreshTurnPhase();
        },
        [this]() {
            m_uiManager.showHUD();
        },
        [this]() {
            updateUIState();
        },
        [this]() {
            saveGame();
        },
        [this]() {
            return captureSelectionBookmark();
        },
        [this](const InputSelectionBookmark& bookmark) {
            reconcileSelectionBookmark(bookmark);
        },
        [this]() {
            m_uiManager.hideGameMenu();
        },
        [this]() {
            m_uiManager.hideMultiplayerAlert();
        },
        [this]() {
            m_uiManager.hideMultiplayerWaitingOverlay();
        },
        [this]() {
            m_uiManager.clearMultiplayerStatus();
        },
        [this]() {
            m_uiManager.showMainMenu();
        }};
}

TurnLifecycleCallbacks Game::makeTurnLifecycleCallbacks() {
    return TurnLifecycleCallbacks{
        [this]() {
            return captureSelectionBookmark();
        },
        [this](const InputSelectionBookmark& bookmark) {
            reconcileSelectionBookmark(bookmark);
        },
        [this]() {
            ensureTurnDraftUpToDate();
        },
        [this]() {
            refreshTurnPhase();
        },
        [this]() {
            updateUIState();
        },
        [this]() {
            saveGame();
            pushSnapshotToRemote(nullptr);
        }};
}

TurnValidationContext Game::authoritativeTurnContext() const {
    return m_engine.makeTurnValidationContext(m_config);
}

CheckTurnValidation Game::validateActivePendingTurn() const {
    return m_engine.validatePendingTurn(m_config);
}

bool Game::canQueueNonMoveActions() const {
    return currentInteractionPermissions().canQueueNonMoveActions;
}

void Game::refreshBuildableCellsOverlay(const InteractionPermissions& permissions) {
    if (m_input.getCurrentTool() != ToolState::Build || !permissions.canShowBuildPreview) {
        m_buildableAnchorCellsOverlay.clear();
        m_buildableCellsOverlay.clear();
        m_buildableCellsOverlayCacheValid = false;
        return;
    }

    const std::uint64_t revision = turnSystem().getPendingStateRevision();
    const int turnNumber = turnSystem().getTurnNumber();
    const KingdomId activeKingdomId = activeKingdom().id;
    const BuildingType buildType = m_input.getBuildPreviewType();
    const int rotationQuarterTurns = m_input.getBuildPreviewRotationQuarterTurns();

    if (m_buildableCellsOverlayCacheValid
        && m_buildableCellsOverlayRevision == revision
        && m_buildableCellsOverlayTurnNumber == turnNumber
        && m_buildableCellsOverlayActiveKingdom == activeKingdomId
        && m_buildableCellsOverlayType == buildType
        && m_buildableCellsOverlayRotationQuarterTurns == rotationQuarterTurns) {
        return;
    }

    if (!permissions.canQueueNonMoveActions) {
        m_buildableAnchorCellsOverlay.clear();
        m_buildableCellsOverlay.clear();
    } else {
        const TurnValidationContext turnContext = authoritativeTurnContext();
        const BuildOverlayRules::BuildOverlayMap buildOverlayMap = BuildOverlayRules::collectBuildOverlayMap(
            turnContext,
            turnSystem().getPendingCommands(),
            buildType,
            rotationQuarterTurns);
        m_buildableAnchorCellsOverlay = std::move(buildOverlayMap.validAnchorCells);
        m_buildableCellsOverlay = std::move(buildOverlayMap.coverageCells);
    }

    m_buildableCellsOverlayRevision = revision;
    m_buildableCellsOverlayTurnNumber = turnNumber;
    m_buildableCellsOverlayActiveKingdom = activeKingdomId;
    m_buildableCellsOverlayType = buildType;
    m_buildableCellsOverlayRotationQuarterTurns = rotationQuarterTurns;
    m_buildableCellsOverlayCacheValid = true;
}

bool Game::shouldUseTurnDraft() const {
    return (m_state == GameState::Playing || m_state == GameState::Paused || m_state == GameState::GameOver)
        && isLocalPlayerTurn()
        && !m_waitingForRemoteTurnResult
        && !turnSystem().getPendingCommands().empty();
}

void Game::invalidateTurnDraft() {
    m_turnDraft.clear();
    m_lastTurnDraftRevision = 0;
}

void Game::ensureTurnDraftUpToDate() {
    const std::uint64_t revision = turnSystem().getPendingStateRevision();
    if (!shouldUseTurnDraft()) {
        if (m_turnDraft.isValid()) {
            const InputSelectionBookmark selectionBookmark = captureSelectionBookmark();
            m_turnDraft.clear();
            m_lastTurnDraftRevision = revision;
            reconcileSelectionBookmark(selectionBookmark);
        } else {
            m_lastTurnDraftRevision = revision;
        }
        return;
    }

    if (m_turnDraft.isValid() && m_lastTurnDraftRevision == revision) {
        return;
    }

    const InputSelectionBookmark selectionBookmark = captureSelectionBookmark();
    std::string errorMessage;
    if (!m_turnDraft.rebuild(m_engine, m_config, turnSystem().getPendingCommands(), &errorMessage)) {
        m_turnDraft.clear();
        m_lastTurnDraftRevision = revision;
        reconcileSelectionBookmark(selectionBookmark);
        return;
    }

    m_lastTurnDraftRevision = revision;
    reconcileSelectionBookmark(selectionBookmark);
}

KingdomId Game::localPerspectiveKingdom() const {
    return FrontendCoordinator::localPerspectiveKingdom(makeFrontendRuntimeState());
}

InGameHudPresentation Game::buildInGameHudPresentation() const {
    return FrontendCoordinator::buildInGameHudPresentation(makeFrontendRuntimeState());
}

bool Game::isMultiplayerSessionReady() const {
    return FrontendCoordinator::isMultiplayerSessionReady(makeFrontendRuntimeState());
}

InteractionPermissions Game::currentInteractionPermissions(const CheckTurnValidation* validation) const {
    const FrontendRuntimeState state = makeFrontendRuntimeState();
    std::optional<CheckTurnValidation> resolvedValidation;
    if (validation) {
        resolvedValidation = *validation;
    } else if (FrontendCoordinator::shouldEvaluateTurnValidation(state)) {
        resolvedValidation = validateActivePendingTurn();
    }

    return FrontendCoordinator::currentInteractionPermissions(state, resolvedValidation);
}

InputContext Game::buildInputContext(const InteractionPermissions& permissions) {
    FrontendDisplayBindings bindings{
        m_window,
        m_camera,
        m_uiManager,
        displayedBoard(),
        displayedKingdoms(),
        displayedPublicBuildings(),
        board(),
        kingdoms(),
        publicBuildings(),
        turnSystem(),
        buildingFactory(),
        authoritativeTurnContext(),
        m_config
    };
    return FrontendCoordinator::buildInputContext(
        makeFrontendRuntimeState(),
        bindings,
        permissions,
        shouldUseTurnDraft() && m_turnDraft.isValid());
}

SelectionQueryView Game::makeSelectionQueryView() {
    return SelectionQueryView{displayedKingdoms(), displayedPublicBuildings()};
}

InputSelectionBookmark Game::captureSelectionBookmark() const {
    return m_input.createSelectionBookmark();
}

Piece* Game::selectedDisplayedPiece() {
    return SelectionQueryCoordinator::findPieceById(makeSelectionQueryView(), m_input.getSelectedPieceId());
}

Building* Game::selectedDisplayedBuilding() {
    return SelectionQueryCoordinator::findBuildingById(makeSelectionQueryView(),
                                                       m_input.getSelectedBuildingId());
}

void Game::reconcileSelectionBookmark(const InputSelectionBookmark& bookmark) {
    const InteractionPermissions permissions = currentInteractionPermissions();
    InputContext inputContext = buildInputContext(permissions);
    const SelectionQueryView queryView = makeSelectionQueryView();
    m_input.reconcileSelection(bookmark,
                               SelectionQueryCoordinator::findPieceById(queryView, bookmark.pieceId),
                               SelectionQueryCoordinator::findBuildingForBookmark(queryView, bookmark),
                               inputContext);
}

void Game::activateSelectTool() {
    m_input.setTool(ToolState::Select);
    m_uiManager.showSelectionEmptyState();
}

Board& Game::displayedBoard() {
    return (m_turnDraft.isValid() && shouldUseTurnDraft()) ? m_turnDraft.board() : board();
}

const Board& Game::displayedBoard() const {
    return (m_turnDraft.isValid() && shouldUseTurnDraft()) ? m_turnDraft.board() : board();
}

std::array<Kingdom, kNumKingdoms>& Game::displayedKingdoms() {
    return (m_turnDraft.isValid() && shouldUseTurnDraft()) ? m_turnDraft.kingdoms() : kingdoms();
}

const std::array<Kingdom, kNumKingdoms>& Game::displayedKingdoms() const {
    return (m_turnDraft.isValid() && shouldUseTurnDraft()) ? m_turnDraft.kingdoms() : kingdoms();
}

std::vector<Building>& Game::displayedPublicBuildings() {
    return (m_turnDraft.isValid() && shouldUseTurnDraft()) ? m_turnDraft.publicBuildings() : publicBuildings();
}

const std::vector<Building>& Game::displayedPublicBuildings() const {
    return (m_turnDraft.isValid() && shouldUseTurnDraft()) ? m_turnDraft.publicBuildings() : publicBuildings();
}

Kingdom& Game::displayedKingdom(KingdomId id) {
    return (m_turnDraft.isValid() && shouldUseTurnDraft()) ? m_turnDraft.kingdom(id) : kingdom(id);
}

const Kingdom& Game::displayedKingdom(KingdomId id) const {
    return (m_turnDraft.isValid() && shouldUseTurnDraft()) ? m_turnDraft.kingdom(id) : kingdom(id);
}

bool Game::isInGameMenuOpen() const {
    return m_uiManager.isGameMenuVisible();
}

void Game::openInGameMenu() {
    const InGameMenuOpenPlan plan = InGamePresentationCoordinator::planOpenInGameMenu(
        makeFrontendRuntimeState());
    if (!plan.shouldOpen) {
        return;
    }

    if (plan.nextGameState.has_value()) {
        m_state = *plan.nextGameState;
    }

    m_uiManager.showGameMenu(plan.presentation);
}

void Game::closeInGameMenu() {
    const InGameMenuClosePlan plan = InGamePresentationCoordinator::planCloseInGameMenu(
        makeFrontendRuntimeState());
    if (!plan.shouldClose) {
        return;
    }

    m_uiManager.hideGameMenu();
    if (plan.nextGameState.has_value()) {
        m_state = *plan.nextGameState;
    }
}

void Game::toggleInGameMenu() {
    if (isInGameMenuOpen()) {
        closeInGameMenu();
    } else {
        openInGameMenu();
    }
}

void Game::returnToMainMenu() {
    m_sessionRuntimeCoordinator.returnToMainMenu(makeSessionRuntimeCallbacks());
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
    const bool hasUnifiedGameConfig = m_config.loadFromFile("assets/config/master_config.json");
    if (!hasUnifiedGameConfig) {
        m_config.loadFromFile("assets/config/game_params.json");
    }

    if (!m_aiDirector.loadConfig("assets/config/master_config.json")) {
        m_aiDirector.loadConfig("assets/config/ai_params.json");
    }

    // Create window
    m_window.create(sf::VideoMode(1280, 720), "A Normal Chess Game", sf::Style::Default);
    m_window.setFramerateLimit(60);
    m_windowSize = m_window.getSize();

#ifdef _WIN32
    installWindowProcHook();
#endif

    m_gui.setTarget(m_window);
    m_gui.setTabKeyUsageEnabled(false);

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
        InteractionPermissions permissions;
        const bool interactiveGameState = m_state == GameState::Playing
            || m_state == GameState::Paused
            || m_state == GameState::GameOver;
        if (interactiveGameState) {
            permissions = currentInteractionPermissions();
        }

        const InputFrameState inputState{
            m_state,
            m_uiManager.isMultiplayerAlertVisible() || m_uiManager.isMultiplayerWaitingOverlayVisible(),
            isInGameMenuOpen(),
            permissions
        };

        const InputPreGuiAction preGuiAction = InputCoordinator::planPreGuiAction(event, inputState);
        switch (preGuiAction.kind) {
            case InputPreGuiActionKind::CloseWindow:
                m_window.close();
                return;

            case InputPreGuiActionKind::ResizeWindow:
                handleWindowResize(preGuiAction.resizedWindow);
                continue;

            case InputPreGuiActionKind::ToggleInGameMenu:
                toggleInGameMenu();
                continue;

            case InputPreGuiActionKind::ExportDebugHistory:
                m_debugRecorder.exportHistory(gameName(),
                                             turnSystem().getTurnNumber(),
                                             turnSystem().getActiveKingdom());
                continue;

            case InputPreGuiActionKind::CommitTurn:
                commitPlayerTurn();
                continue;

            case InputPreGuiActionKind::CenterCameraOnPerspective:
                centerCameraOnKingdom(localPerspectiveKingdom());
                continue;

            case InputPreGuiActionKind::ActivateSelectTool:
                activateSelectTool();
                continue;

            case InputPreGuiActionKind::SkipEvent:
                continue;

            case InputPreGuiActionKind::None:
            default:
                break;
        }

        const bool handledByGui = m_gui.handleEvent(event);

        bool worldMouseBlocked = false;
        if (!inputState.overlaysVisible
            && !handledByGui
            && InputCoordinator::isInteractiveGameState(inputState)) {
            const auto mouseScreenPos = InputCoordinator::mouseScreenPositionFromEvent(event);
            if (mouseScreenPos) {
                worldMouseBlocked = m_uiManager.blocksWorldMouseInput(*mouseScreenPos, m_windowSize);
            }
        }

        const InputPostGuiAction postGuiAction =
            InputCoordinator::planPostGuiAction(inputState, handledByGui, worldMouseBlocked);
        if (postGuiAction.kind == InputPostGuiActionKind::DispatchToWorld) {
            ensureTurnDraftUpToDate();
            InputContext inputContext = buildInputContext(inputState.permissions);
            m_input.handleEvent(event, inputContext);
        }
    }
}

void Game::update() {
    ensureTurnDraftUpToDate();
    const FrameUpdatePlan updatePlan = UpdateCoordinator::planFrameUpdate(FrameUpdateState{
        m_state,
        currentInteractionPermissions().canMoveCamera,
        isLanHost(),
        isLanClient(),
        m_engine.isActiveAI()
    });

    if (updatePlan.updateCamera) {
        m_input.updateCameraMovement(m_clock.getDeltaTime(), m_camera);
    }

    if (updatePlan.updateMultiplayer) {
        updateMultiplayer();
    }

    if (updatePlan.runAITurn) {
        m_aiTurnCoordinator.startTurnIfNeeded(m_state);
        if (m_aiTurnCoordinator.pollCompletedTurn(m_state)) {
            commitAuthoritativeTurn();
        }
    }
    if (updatePlan.updateUI) {
        updateUIState();
    }
    if (updatePlan.updateUIManager) {
        m_uiManager.update();
    }
}

void Game::render() {
    m_window.clear(sf::Color(30, 30, 30));

    if (RenderCoordinator::shouldRenderWorld(m_state)) {
        ensureTurnDraftUpToDate();
        const bool usingConcretePendingState = shouldUseTurnDraft() && m_turnDraft.isValid();
        const InteractionPermissions renderPermissions = currentInteractionPermissions();
        refreshBuildableCellsOverlay(renderPermissions);
        WorldRenderState renderState;
        renderState.gameState = m_state;
        renderState.activeTool = m_input.getCurrentTool();
        renderState.permissions = renderPermissions;
        renderState.usingConcretePendingState = usingConcretePendingState;
        renderState.activeKingdom = activeKingdom().id;
        renderState.selectedPiece = selectedDisplayedPiece();
        renderState.selectedBuilding = selectedDisplayedBuilding();
        if (m_input.hasSelectedCell()) {
            renderState.selectedCell = m_input.getSelectedCell();
        }
        renderState.selectedOriginDangerous = m_input.isSelectedOriginDangerous();
        renderState.validMoves = m_input.getValidMoves();
        renderState.dangerMoves = m_input.getDangerMoves();
        renderState.capturePreviewPieceIds = m_input.getCapturePreviewPieceIds();
        renderState.hasBuildPreview = m_input.hasBuildPreview();
        renderState.buildPreviewType = m_input.getBuildPreviewType();
        renderState.buildPreviewAnchorCell = m_input.getBuildPreviewAnchorCell();
        renderState.buildPreviewRotationQuarterTurns = m_input.getBuildPreviewRotationQuarterTurns();

        WorldRenderBindings bindings{
            m_window,
            m_hudView,
            m_windowSize,
            m_camera,
            m_renderer,
            m_assets,
            displayedBoard(),
            displayedKingdoms(),
            displayedPublicBuildings(),
            m_config
        };
        const WorldRenderPlan renderPlan = RenderCoordinator::buildWorldRenderPlan(
            renderState,
            turnSystem().getPendingCommands(),
            m_buildableAnchorCellsOverlay,
            m_buildableCellsOverlay,
            m_config);
        RenderCoordinator::renderWorldFrame(bindings, renderPlan);
    }

    m_window.setView(m_hudView);
    m_gui.draw();
    m_window.display();
}

bool Game::startNewGame(const GameSessionConfig& session, std::string* errorMessage) {
    return m_sessionRuntimeCoordinator.startNewGame(session, makeSessionRuntimeCallbacks(), errorMessage);
}

bool Game::loadGame(const std::string& saveName) {
    std::string error;
    if (!m_sessionRuntimeCoordinator.loadGame(saveName, makeSessionRuntimeCallbacks(), &error)) {
        std::cerr << "Failed to restore save: " << error << std::endl;
        return false;
    }
    return true;
}

bool Game::saveGame() {
    std::string error;
    if (!m_sessionFlow.saveAuthoritativeSession(!isLanClient(), &error)) {
        std::cerr << error << std::endl;
        return false;
    }

    return true;
}

void Game::commitPlayerTurn() {
    m_turnLifecycleCoordinator.commitPlayerTurn(
        isLanClient(),
        isLanHost(),
        m_multiplayer.clientIsConnected(),
        makeTurnLifecycleCallbacks());
}

void Game::commitAuthoritativeTurn() {
    m_turnLifecycleCoordinator.commitAuthoritativeTurn(isLanHost(), makeTurnLifecycleCallbacks());
}

void Game::resetPlayerTurn() {
    m_turnLifecycleCoordinator.resetPlayerTurn(makeTurnLifecycleCallbacks());
}

void Game::stopMultiplayer() {
    m_multiplayer.resetConnections();
    m_waitingForRemoteTurnResult = false;
    m_localPlayerContext = LocalPlayerContext{};
    m_multiplayer.clearReconnectState();
    m_uiManager.hideMultiplayerWaitingOverlay();
    m_uiManager.hideMultiplayerAlert();
    m_uiManager.clearMultiplayerStatus();
}

bool Game::pushSnapshotToRemote(std::string* errorMessage) {
    const std::string snapshot = m_saveManager.serialize(m_engine.createSaveData());
    return m_multiplayer.pushSnapshotIfConnected(isLanHost(), snapshot, errorMessage);
}

bool Game::joinMultiplayer(const JoinMultiplayerRequest& request, std::string* errorMessage) {
    return m_sessionRuntimeCoordinator.joinMultiplayer(request, makeSessionRuntimeCallbacks(), errorMessage);
}

bool Game::reconnectToMultiplayerHost(std::string* errorMessage) {
    return m_sessionRuntimeCoordinator.reconnectToMultiplayerHost(
        makeSessionRuntimeCallbacks(),
        errorMessage);
}

void Game::updateMultiplayer() {
    const MultiplayerRuntimeCallbacks callbacks{
        [this](std::string* errorMessage) {
            return pushSnapshotToRemote(errorMessage);
        },
        [this](const std::vector<TurnCommand>& commands, std::string* errorMessage) {
            return m_turnLifecycleCoordinator.applyRemoteTurnSubmission(
                isLanHost(),
                commands,
                makeTurnLifecycleCallbacks(),
                errorMessage);
        },
        [this]() {
            return captureSelectionBookmark();
        },
        [this](const InputSelectionBookmark& bookmark) {
            reconcileSelectionBookmark(bookmark);
        },
        [this]() {
            refreshTurnPhase();
        },
        [this]() {
            updateUIState();
        },
        [this]() {
            invalidateTurnDraft();
        },
        [this]() {
            returnToMainMenu();
        },
        [this](std::string* errorMessage) {
            return reconnectToMultiplayerHost(errorMessage);
        }};
    m_multiplayerRuntimeCoordinator.update(callbacks);
}

void Game::setupUICallbacks() {
    UICallbackCoordinator::configure(makeUICallbackCoordinatorDependencies());
}

void Game::updateUIState() {
    ensureTurnDraftUpToDate();
    turnSystem().syncPointBudget(m_config);
    const FrontendRuntimeState runtimeState = makeFrontendRuntimeState();
    const bool usingProjectedDisplayState = m_turnDraft.isValid() && shouldUseTurnDraft();
    const Board& currentDisplayedBoard = displayedBoard();
    const auto& currentDisplayedKingdoms = displayedKingdoms();
    const auto& currentDisplayedPublicBuildings = displayedPublicBuildings();
    const CheckTurnValidation validation = validateActivePendingTurn();
    const InteractionPermissions permissions = currentInteractionPermissions(&validation);
    const InGameHudPresentation hudPresentation = buildInGameHudPresentation();
    const Cell* selectedCell = nullptr;
    if (m_input.hasSelectedCell()) {
        const sf::Vector2i cellPos = m_input.getSelectedCell();
        selectedCell = &currentDisplayedBoard.getCell(cellPos.x, cellPos.y);
    }
    InGamePresentationCoordinator::updateInGameUi(
        m_uiManager,
        InGamePresentationState{
            runtimeState,
            usingProjectedDisplayState,
            m_input.getCurrentTool(),
            m_uiManager.isRightSidebarVisible(),
            selectedDisplayedPiece(),
            selectedDisplayedBuilding(),
            selectedCell
        },
        InGamePresentationBindings{
            m_engine,
            currentDisplayedBoard,
            currentDisplayedKingdoms,
            currentDisplayedPublicBuildings,
            turnSystem(),
            m_config
        },
        permissions,
        validation,
        hudPresentation,
        [this]() {
            returnToMainMenu();
        });
}

void Game::refreshTurnPhase() {
    m_turnPhase = (turnSystem().getActiveKingdom() == KingdomId::White)
        ? TurnPhase::WhiteTurn
        : TurnPhase::BlackTurn;
}

void Game::discardPendingAITurn() {
    m_aiTurnCoordinator.cancelPendingTurn();
}
