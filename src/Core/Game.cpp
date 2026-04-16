#include "Core/Game.hpp"
#include "Render/OverlayRenderer.hpp"
#include "Render/StructureOverlay.hpp"
#include "Save/SaveData.hpp"
#include "Buildings/BuildingType.hpp"
#include "Buildings/StructurePlacementProfile.hpp"
#include "Systems/BuildOverlayRules.hpp"
#include "Systems/BuildSystem.hpp"
#include "Systems/PendingTurnProjection.hpp"
#include "Systems/PublicBuildingOccupation.hpp"
#include "Input/InputContext.hpp"
#include "UI/InGameViewModelBuilder.hpp"
#include "Multiplayer/PasswordUtils.hpp"
#include <algorithm>
#include <iostream>
#include <optional>
#include <thread>
#include <ctime>

namespace {

void drawStructureOverlaysForBuildings(sf::RenderWindow& window,
                                       Renderer& renderer,
                                       const Camera& camera,
                                       const sf::View& hudView,
                                       sf::Vector2u windowSize,
                                       const std::vector<Building>& buildings,
                                       const Board& board,
                                       const GameConfig& config,
                                       const AssetManager& assets,
                                       const StructureOverlayPolicy& overlayPolicy,
                                       const Building* selectedBuilding) {
    for (const Building& building : buildings) {
        StructureOverlayContext overlayContext;
        overlayContext.isSelected = selectedBuilding == &building;

        const StructureOverlayStack overlay = buildStructureOverlay(
            building, board, config, overlayContext, overlayPolicy);
        if (overlay.isEmpty()) {
            continue;
        }

        renderer.getOverlay().drawStructureOverlay(
            window, camera, hudView, windowSize,
            building, overlay, config.getCellSizePx(), assets);
    }
}

int countDisplayedOccupiedBuildingCells(const Kingdom& kingdom) {
    int occupiedCells = 0;
    for (const Building& building : kingdom.buildings) {
        if (building.isDestroyed()) {
            continue;
        }

        occupiedCells += static_cast<int>(building.getOccupiedCells().size());
    }

    return occupiedCells;
}

bool isBlockedGameplayShortcutKey(sf::Keyboard::Key key) {
    return key == sf::Keyboard::Escape
        || key == sf::Keyboard::P
        || key == sf::Keyboard::K
        || key == sf::Keyboard::Space;
}

bool isBlockedGuiNavigationKey(const sf::Event& event) {
    if (event.type != sf::Event::KeyPressed && event.type != sf::Event::KeyReleased) {
        return false;
    }

    return event.key.code == sf::Keyboard::Tab;
}

std::optional<sf::Vector2i> mouseScreenPositionFromEvent(const sf::Event& event) {
    switch (event.type) {
        case sf::Event::MouseMoved:
            return sf::Vector2i(event.mouseMove.x, event.mouseMove.y);
        case sf::Event::MouseButtonPressed:
        case sf::Event::MouseButtonReleased:
            return sf::Vector2i(event.mouseButton.x, event.mouseButton.y);
        case sf::Event::MouseWheelScrolled:
            return sf::Vector2i(event.mouseWheelScroll.x, event.mouseWheelScroll.y);
        default:
            return std::nullopt;
    }
}

void writeMultiplayerError(std::string* errorMessage, const std::string& message) {
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool waitForServerInfo(MultiplayerClient& client,
                       MultiplayerServerInfo& serverInfo,
                       sf::Time timeout,
                       std::string* errorMessage) {
    sf::Clock clock;
    while (clock.getElapsedTime() < timeout) {
        client.update();
        while (client.hasPendingEvent()) {
            const auto event = client.popNextEvent();
            if (event.type == MultiplayerClient::Event::Type::ServerInfoReceived) {
                serverInfo = event.serverInfo;
                return true;
            }

            if (event.type == MultiplayerClient::Event::Type::Disconnected
                || event.type == MultiplayerClient::Event::Type::Error) {
                writeMultiplayerError(
                    errorMessage,
                    event.message.empty() ? "The multiplayer host did not respond." : event.message);
                client.disconnect();
                return false;
            }
        }
    }

    writeMultiplayerError(errorMessage, "The multiplayer host did not answer the ping request.");
    client.disconnect();
    return false;
}

bool waitForJoinSnapshot(MultiplayerClient& client,
                         SaveManager& saveManager,
                         SaveData& snapshotData,
                         sf::Time timeout,
                         std::string* errorMessage) {
    bool accepted = false;
    bool receivedSnapshot = false;
    sf::Clock clock;
    while (clock.getElapsedTime() < timeout && !receivedSnapshot) {
        client.update();
        while (client.hasPendingEvent()) {
            const auto event = client.popNextEvent();
            if (event.type == MultiplayerClient::Event::Type::JoinRejected) {
                writeMultiplayerError(
                    errorMessage,
                    event.message.empty() ? "The multiplayer host rejected the join request." : event.message);
                client.disconnect();
                return false;
            }

            if (event.type == MultiplayerClient::Event::Type::JoinAccepted) {
                accepted = true;
                continue;
            }

            if (event.type == MultiplayerClient::Event::Type::SnapshotReceived) {
                if (!saveManager.deserialize(event.serializedSaveData, snapshotData)) {
                    writeMultiplayerError(errorMessage, "The multiplayer host sent an invalid game snapshot.");
                    client.disconnect();
                    return false;
                }

                receivedSnapshot = true;
                break;
            }

            if (event.type == MultiplayerClient::Event::Type::Disconnected
                || event.type == MultiplayerClient::Event::Type::Error) {
                writeMultiplayerError(
                    errorMessage,
                    event.message.empty() ? "Lost connection to the multiplayer host." : event.message);
                client.disconnect();
                return false;
            }
        }
    }

    if (!accepted || !receivedSnapshot) {
        writeMultiplayerError(errorMessage, "The multiplayer host did not complete the join handshake in time.");
        client.disconnect();
        return false;
    }

    return true;
}

}

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
        case TurnCommand::Disband: return "Disband";
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

void Game::cacheReconnectRequest(const JoinMultiplayerRequest& request) {
    m_clientReconnectState.available = true;
    m_clientReconnectState.awaitingReconnect = false;
    m_clientReconnectState.reconnectAttemptInProgress = false;
    m_clientReconnectState.lastErrorMessage.clear();
    m_clientReconnectState.request = request;
}

void Game::clearReconnectState() {
    m_clientReconnectState = {};
}

void Game::configureLocalPlayerContext(const GameSessionConfig& session) {
    m_localPlayerContext = makeLocalPlayerContextForSession(session);
}

bool Game::isLocalPlayerTurn() const {
    return m_localPlayerContext.isLocallyControlled(turnSystem().getActiveKingdom());
}

bool Game::canLocalPlayerIssueCommands() const {
    return currentInteractionPermissions().canIssueCommands;
}

CheckTurnValidation Game::validateActivePendingTurn() const {
    return CheckResponseRules::validatePendingTurn(
        activeKingdom(), enemyKingdom(), board(), publicBuildings(),
        turnSystem().getTurnNumber(), turnSystem().getPendingCommands(), m_config);
}

bool Game::isActiveKingInCheckForRules() const {
    return CheckResponseRules::isActiveKingInCheck(
        activeKingdom(), enemyKingdom(), board(), publicBuildings(),
        turnSystem().getTurnNumber(), turnSystem().getPendingCommands(), m_config);
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
        const BuildOverlayRules::BuildOverlayMap buildOverlayMap = BuildOverlayRules::collectBuildOverlayMap(
            board(),
            activeKingdom(),
            enemyKingdom(),
            publicBuildings(),
            turnNumber,
            turnSystem().getPendingCommands(),
            buildType,
            rotationQuarterTurns,
            m_config);
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
    if (isLocalPlayerTurn()) {
        return turnSystem().getActiveKingdom();
    }

    return m_localPlayerContext.perspectiveKingdom;
}

InGameHudPresentation Game::buildInGameHudPresentation() const {
    InGameHudPresentation presentation;
    const bool singleLocalKingdom = hasSingleLocallyControlledKingdom(m_localPlayerContext);

    presentation.statsKingdom = singleLocalKingdom
        ? localPerspectiveKingdom()
        : turnSystem().getActiveKingdom();
    presentation.showTurnPointIndicators = !singleLocalKingdom || isLocalPlayerTurn();

    if (singleLocalKingdom
        && isLocalPlayerTurn()
        && !m_waitingForRemoteTurnResult
        && isMultiplayerSessionReady()
        && (m_state == GameState::Playing || m_state == GameState::Paused)) {
        presentation.turnIndicatorTone = InGameTurnIndicatorTone::LocalTurn;
    }

    return presentation;
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

InteractionPermissions Game::currentInteractionPermissions(const CheckTurnValidation* validation) const {
    InteractionPermissionInputs inputs;
    inputs.gameState = m_state;
    inputs.overlaysVisible = m_uiManager.isMultiplayerAlertVisible()
        || m_uiManager.isMultiplayerWaitingOverlayVisible();
    inputs.inGameMenuOpen = isInGameMenuOpen();
    inputs.waitingForRemoteTurnResult = m_waitingForRemoteTurnResult;
    inputs.multiplayerSessionReady = isMultiplayerSessionReady();
    inputs.isLocalPlayerTurn = isLocalPlayerTurn();

    const bool shouldEvaluateTurnValidation = inputs.gameState == GameState::Playing
        && !inputs.overlaysVisible
        && !inputs.waitingForRemoteTurnResult
        && inputs.multiplayerSessionReady
        && inputs.isLocalPlayerTurn;
    if (validation) {
        inputs.activeKingInCheck = validation->activeKingInCheck;
        inputs.projectedKingInCheck = validation->projectedKingInCheck;
        inputs.hasAnyLegalResponse = validation->hasAnyLegalResponse;
    } else if (shouldEvaluateTurnValidation) {
        const CheckTurnValidation liveValidation = validateActivePendingTurn();
        inputs.activeKingInCheck = liveValidation.activeKingInCheck;
        inputs.projectedKingInCheck = liveValidation.projectedKingInCheck;
        inputs.hasAnyLegalResponse = liveValidation.hasAnyLegalResponse;
    }

    return computeInteractionPermissions(inputs);
}

InputContext Game::buildInputContext(const InteractionPermissions& permissions) {
    Board& currentBoard = displayedBoard();
    const KingdomId perspectiveKingdom = permissions.canIssueCommands
        ? turnSystem().getActiveKingdom()
        : localPerspectiveKingdom();
    Kingdom& selectableKingdom = permissions.canIssueCommands
        ? displayedKingdom(turnSystem().getActiveKingdom())
        : displayedKingdom(perspectiveKingdom);
    Kingdom& opposingKingdom = permissions.canIssueCommands
        ? displayedKingdom(opponent(turnSystem().getActiveKingdom()))
        : displayedKingdom(opponent(perspectiveKingdom));
    const bool materializePendingStateLocally = (m_state == GameState::Playing
            || m_state == GameState::Paused
            || m_state == GameState::GameOver)
        && isLocalPlayerTurn()
        && !m_waitingForRemoteTurnResult;
    const bool useConcretePendingState = shouldUseTurnDraft() && m_turnDraft.isValid();

    return {
        m_window,
        m_camera,
        currentBoard,
        turnSystem(),
        buildingFactory(),
        selectableKingdom,
        opposingKingdom,
        displayedPublicBuildings(),
        board(),
        activeKingdom(),
        enemyKingdom(),
        publicBuildings(),
        m_uiManager,
        m_config,
        permissions,
        materializePendingStateLocally,
        useConcretePendingState
    };
}

InputSelectionBookmark Game::captureSelectionBookmark() const {
    return m_input.createSelectionBookmark();
}

Piece* Game::findPieceById(int pieceId) {
    if (pieceId < 0) {
        return nullptr;
    }

    if (Piece* piece = displayedKingdom(KingdomId::White).getPieceById(pieceId)) {
        return piece;
    }
    return displayedKingdom(KingdomId::Black).getPieceById(pieceId);
}

Building* Game::findBuildingById(int buildingId) {
    for (Building& building : displayedPublicBuildings()) {
        if (building.id == buildingId) {
            return &building;
        }
    }
    for (Building& building : displayedKingdom(KingdomId::White).buildings) {
        if (building.id == buildingId) {
            return &building;
        }
    }
    for (Building& building : displayedKingdom(KingdomId::Black).buildings) {
        if (building.id == buildingId) {
            return &building;
        }
    }

    return nullptr;
}

Building* Game::findBuildingForBookmark(const InputSelectionBookmark& bookmark) {
    if (Building* building = findBuildingById(bookmark.buildingId)) {
        return building;
    }

    if (!bookmark.selectedBuildingOrigin.has_value()) {
        return nullptr;
    }

    const auto matchesBookmark = [&bookmark](Building& building) {
        return building.origin == *bookmark.selectedBuildingOrigin
            && building.type == bookmark.selectedBuildingType
            && building.isNeutral == bookmark.selectedBuildingIsNeutral
            && building.owner == bookmark.selectedBuildingOwner
            && building.rotationQuarterTurns == bookmark.selectedBuildingRotationQuarterTurns;
    };

    for (Building& building : displayedPublicBuildings()) {
        if (matchesBookmark(building)) {
            return &building;
        }
    }
    for (Building& building : displayedKingdom(KingdomId::White).buildings) {
        if (matchesBookmark(building)) {
            return &building;
        }
    }
    for (Building& building : displayedKingdom(KingdomId::Black).buildings) {
        if (matchesBookmark(building)) {
            return &building;
        }
    }

    return nullptr;
}

void Game::reconcileSelectionBookmark(const InputSelectionBookmark& bookmark) {
    const InteractionPermissions permissions = currentInteractionPermissions();
    InputContext inputContext = buildInputContext(permissions);
    m_input.reconcileSelection(bookmark,
                               findPieceById(bookmark.pieceId),
                               findBuildingForBookmark(bookmark),
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
    if (m_state != GameState::Playing && m_state != GameState::Paused && m_state != GameState::GameOver) {
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
            if (m_uiManager.isMultiplayerAlertVisible()) {
                m_uiManager.hideMultiplayerWaitingOverlay();
                m_uiManager.setMultiplayerStatus("LAN Host | Waiting for Black", MultiplayerStatusTone::Waiting);
                return;
            }

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
        if (m_clientReconnectState.awaitingReconnect) {
            m_uiManager.setMultiplayerStatus(
                m_clientReconnectState.available
                    ? "LAN Client | Disconnected - reconnect available"
                    : "LAN Client | Connection lost",
                MultiplayerStatusTone::Waiting);
            return;
        }

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
    invalidateTurnDraft();
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
        if (event.type == sf::Event::Closed) {
            m_window.close();
            return;
        }

        if (event.type == sf::Event::Resized) {
            sf::Vector2u newSize(event.size.width, event.size.height);
            handleWindowResize(newSize);
            continue;
        }

        const bool inInteractiveGameState = (m_state == GameState::Playing
                                          || m_state == GameState::Paused
                                          || m_state == GameState::GameOver);
        const bool overlaysVisible = m_uiManager.isMultiplayerAlertVisible()
                                  || m_uiManager.isMultiplayerWaitingOverlayVisible();

        if (inInteractiveGameState && !overlaysVisible
            && event.type == sf::Event::KeyPressed
            && isBlockedGameplayShortcutKey(event.key.code)) {
            if (event.key.code == sf::Keyboard::Escape) {
                toggleInGameMenu();
                continue;
            }

            if (event.key.code == sf::Keyboard::P) {
                m_debugRecorder.exportHistory(gameName(),
                                             turnSystem().getTurnNumber(),
                                             turnSystem().getActiveKingdom());
                continue;
            }

            if (isInGameMenuOpen()) {
                continue;
            }

            if (event.key.code == sf::Keyboard::Space) {
                if (canLocalPlayerIssueCommands()) {
                    commitPlayerTurn();
                }
                continue;
            }

            if (event.key.code == sf::Keyboard::K) {
                centerCameraOnKingdom(localPerspectiveKingdom());
                continue;
            }
        }

        if (inInteractiveGameState && !overlaysVisible
            && !isInGameMenuOpen()
            && event.type == sf::Event::MouseButtonPressed
            && event.mouseButton.button == sf::Mouse::Right) {
            if (currentInteractionPermissions().canUseToolbar) {
                activateSelectTool();
            }
            continue;
        }

        if (isBlockedGuiNavigationKey(event)) {
            continue;
        }

        const bool handledByGui = m_gui.handleEvent(event);

        if (overlaysVisible) {
            continue;
        }

        if (handledByGui) {
            continue;
        }

        if (inInteractiveGameState) {
            const auto mouseScreenPos = mouseScreenPositionFromEvent(event);
            if (mouseScreenPos && m_uiManager.blocksWorldMouseInput(*mouseScreenPos, m_windowSize)) {
                continue;
            }
        }

        if (isInGameMenuOpen()) {
            continue;
        }

        const InteractionPermissions permissions = currentInteractionPermissions();
        if (permissions.canInspectWorld) {
            ensureTurnDraftUpToDate();
            InputContext inputContext = buildInputContext(permissions);
            m_input.handleEvent(event, inputContext);
        }
    }
}

void Game::update() {
    ensureTurnDraftUpToDate();

    if (currentInteractionPermissions().canMoveCamera) {
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
        case GameState::Paused:
        case GameState::GameOver:
            updateUIState();
            m_uiManager.update();
            break;
        default:
            break;
    }
}

void Game::render() {
    m_window.clear(sf::Color(30, 30, 30));

    if (m_state == GameState::Playing || m_state == GameState::Paused || m_state == GameState::GameOver) {
        ensureTurnDraftUpToDate();

        m_camera.applyTo(m_window);
        m_renderer.setSkipPieceIds(m_input.getCapturePreviewPieceIds());
        m_renderer.drawWorldBase(m_window, m_camera, displayedBoard(), displayedKingdoms(),
            displayedPublicBuildings());
        const bool usingConcretePendingState = shouldUseTurnDraft() && m_turnDraft.isValid();

        const InteractionPermissions renderPermissions = currentInteractionPermissions();
        const bool showActionOverlays = renderPermissions.canShowActionOverlays;
        const bool showBuildPreview = renderPermissions.canShowBuildPreview;
        const Piece* selectedPiece = m_input.getSelectedPiece();
        const bool canShowSelectedPieceActions = showActionOverlays
            && selectedPiece
            && selectedPiece->kingdom == activeKingdom().id;

        if (m_input.getCurrentTool() == ToolState::Select && selectedPiece) {
            m_renderer.getOverlay().drawOrientationCheckerboard(
                m_window, displayedBoard(), m_config.getCellSizePx());
        }

        refreshBuildableCellsOverlay(renderPermissions);

        if (canShowSelectedPieceActions && m_input.getCurrentTool() == ToolState::Select) {
            const TurnCommand* pendingMove = turnSystem().getPendingMoveCommand(selectedPiece->id);
            const sf::Vector2i highlightedOrigin = pendingMove
                ? pendingMove->origin
                : selectedPiece->position;
            const bool shouldShowOriginOverlay = (selectedPiece->type == PieceType::King) || (pendingMove != nullptr);
            if (shouldShowOriginOverlay) {
                const sf::Color originColor = m_input.isSelectedOriginDangerous()
                    ? sf::Color(255, 40, 40, 90)
                    : sf::Color(40, 120, 255, 130);
                m_renderer.getOverlay().drawOriginCell(m_window, m_camera,
                    highlightedOrigin, m_config.getCellSizePx(), originColor);
            }
            m_renderer.getOverlay().drawReachableCells(m_window, m_camera,
                m_input.getValidMoves(), m_config.getCellSizePx());
            if (!m_input.getDangerMoves().empty()) {
                m_renderer.getOverlay().drawDangerCells(m_window, m_camera,
                    m_input.getDangerMoves(), m_config.getCellSizePx());
            }
        } else if (m_input.getCurrentTool() == ToolState::Build && !m_buildableCellsOverlay.empty()) {
            m_renderer.getOverlay().drawReachableCells(
                m_window,
                m_camera,
                m_buildableCellsOverlay,
                m_config.getCellSizePx());
        }

        m_renderer.drawPiecesLayer(m_window, m_camera, displayedKingdoms());

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
                    selectedBuilding->getFootprintWidth(), selectedBuilding->getFootprintHeight(),
                    m_config.getCellSizePx());
            } else if (m_input.hasSelectedCell()) {
                m_renderer.getOverlay().drawSelectionFrame(m_window, m_camera,
                    m_hudView, m_windowSize, m_input.getSelectedCell(),
                    1, 1, m_config.getCellSizePx());
            }
        }
        if (showBuildPreview && m_input.getCurrentTool() == ToolState::Build && m_input.hasBuildPreview()) {
            BuildingType bt = m_input.getBuildPreviewType();
            const int previewRotationQuarterTurns = m_input.getBuildPreviewRotationQuarterTurns();
            const sf::Vector2i previewAnchorCell = m_input.getBuildPreviewAnchorCell();
            const sf::Vector2i previewOrigin = StructurePlacementProfiles::originFromAnchorCell(
                bt,
                previewAnchorCell,
                previewRotationQuarterTurns,
                m_config);
            const int bw = m_config.getBuildingWidth(bt);
            const int bh = m_config.getBuildingHeight(bt);
            const bool valid = renderPermissions.canQueueNonMoveActions
                && std::find(m_buildableAnchorCellsOverlay.begin(),
                             m_buildableAnchorCellsOverlay.end(),
                             previewAnchorCell) != m_buildableAnchorCellsOverlay.end();
            m_renderer.getOverlay().drawBuildPreview(m_window, m_camera,
                m_hudView, m_windowSize,
                previewOrigin, bt, bw, bh, previewRotationQuarterTurns,
                0, m_config.getCellSizePx(), valid, m_assets);
        }
        if (showActionOverlays && !usingConcretePendingState) {
            for (const TurnCommand& pendingCommand : turnSystem().getPendingCommands()) {
                if (pendingCommand.type != TurnCommand::Build) {
                    continue;
                }

                const int bw = m_config.getBuildingWidth(pendingCommand.buildingType);
                const int bh = m_config.getBuildingHeight(pendingCommand.buildingType);
                m_renderer.getOverlay().drawBuildPreview(m_window, m_camera,
                    m_hudView, m_windowSize,
                    pendingCommand.buildOrigin, pendingCommand.buildingType, bw, bh,
                    pendingCommand.buildRotationQuarterTurns, 0,
                    m_config.getCellSizePx(), true, m_assets);
                m_renderer.getOverlay().drawActionMarker(m_window,
                    m_camera,
                    m_hudView,
                    m_windowSize,
                    pendingCommand.buildOrigin,
                    Building::getFootprintWidthFor(bw, bh, pendingCommand.buildRotationQuarterTurns),
                    Building::getFootprintHeightFor(bw, bh, pendingCommand.buildRotationQuarterTurns),
                    "build_ongoing",
                    m_config.getCellSizePx(),
                    m_assets);
            }
        }

        if (showActionOverlays) {
            for (const TurnCommand& pendingCommand : turnSystem().getPendingCommands()) {
                if (pendingCommand.type != TurnCommand::Move) {
                    continue;
                }

                m_renderer.getOverlay().drawActionMarker(m_window,
                    m_camera,
                    m_hudView,
                    m_windowSize,
                    pendingCommand.destination,
                    1,
                    1,
                    "move_ongoing",
                    m_config.getCellSizePx(),
                    m_assets);
            }
        }
        const StructureOverlayPolicy overlayPolicy = makeWorldStructureOverlayPolicy();
        const Building* selectedBuilding = m_input.getSelectedBuilding();
        drawStructureOverlaysForBuildings(
            m_window, m_renderer, m_camera, m_hudView, m_windowSize,
            displayedPublicBuildings(), displayedBoard(), m_config, m_assets, overlayPolicy, selectedBuilding);
        for (const Kingdom& kingdomState : displayedKingdoms()) {
            drawStructureOverlaysForBuildings(
                m_window, m_renderer, m_camera, m_hudView, m_windowSize,
                kingdomState.buildings, displayedBoard(), m_config, m_assets, overlayPolicy, selectedBuilding);
        }
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
    invalidateTurnDraft();

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
    invalidateTurnDraft();

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
            if (!error.empty() && m_multiplayerClient.isConnected()) {
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
    const InputSelectionBookmark selectionBookmark = captureSelectionBookmark();

    const CheckTurnValidation validation = validateActivePendingTurn();
    if (!validation.valid) {
        if (validation.activeKingInCheck && !validation.hasAnyLegalResponse) {
            m_state = GameState::GameOver;
            eventLog().log(turnSystem().getTurnNumber(), enemyId,
                           "Checkmate! " + participantName(enemyId) + " wins!");
            m_input.clearMovePreview();
            reconcileSelectionBookmark(selectionBookmark);
            if (isLanHost()) {
                saveGame();
                pushSnapshotToRemote(nullptr);
            }
            updateUIState();
        }
        return;
    }

    turnSystem().commitTurn(board(), activeKingdom(), enemyKingdom(),
                            publicBuildings(), m_config, eventLog(),
                            pieceFactory(), buildingFactory());
    m_debugRecorder.logTurnState(turnSystem().getTurnNumber(), kingdoms(), "after player commit");
    m_debugRecorder.recordSnapshot(turnSystem().getTurnNumber(),
                                   turnSystem().getActiveKingdom(),
                                   kingdoms(),
                                   "after_player_commit");

    turnSystem().advanceTurn();
    m_input.clearMovePreview();
    m_waitingForRemoteTurnResult = false;
    refreshTurnPhase();
    ensureTurnDraftUpToDate();
    reconcileSelectionBookmark(selectionBookmark);

    const CheckTurnValidation enemyValidation = validateActivePendingTurn();
    if (enemyValidation.activeKingInCheck && !enemyValidation.hasAnyLegalResponse) {
        m_state = GameState::GameOver;
        std::string winner = participantName(activeId);
        eventLog().log(turnSystem().getTurnNumber(), activeId,
                       "Checkmate! " + winner + " wins!");
        reconcileSelectionBookmark(selectionBookmark);
        if (isLanHost()) {
            saveGame();
            pushSnapshotToRemote(nullptr);
        }
        updateUIState();
        return;
    }

    if (isLanHost()) {
        saveGame();
        pushSnapshotToRemote(nullptr);
    }
    startAITurnIfNeeded();
}

void Game::resetPlayerTurn() {
    const InputSelectionBookmark selectionBookmark = captureSelectionBookmark();
    m_input.cancelLiveMove(activeKingdom(), turnSystem());  // restore piece positions for queued move previews
    turnSystem().resetPendingCommands();
    ensureTurnDraftUpToDate();
    reconcileSelectionBookmark(selectionBookmark);
}

void Game::stopMultiplayer() {
    if (m_multiplayerServer.isRunning()) {
        m_multiplayerServer.sendDisconnectNotice("Host closed the multiplayer session.", nullptr);
    }

    m_multiplayerServer.stop();
    m_multiplayerClient.disconnect();
    m_lanHostRemoteSessionEstablished = false;
    m_waitingForRemoteTurnResult = false;
    m_multiplayerHostJoinHint.clear();
    m_localPlayerContext = LocalPlayerContext{};
    clearReconnectState();
    m_uiManager.hideMultiplayerWaitingOverlay();
    m_uiManager.hideMultiplayerAlert();
    m_uiManager.clearMultiplayerStatus();
}

void Game::prepareForClientConnectionAttempt(bool preserveLanClientContext) {
    if (m_multiplayerServer.isRunning()) {
        m_multiplayerServer.sendDisconnectNotice("Host closed the multiplayer session.", nullptr);
    }

    m_multiplayerServer.stop();
    m_multiplayerClient.disconnect();
    m_lanHostRemoteSessionEstablished = false;
    m_waitingForRemoteTurnResult = false;
    m_multiplayerHostJoinHint.clear();
    if (!preserveLanClientContext) {
        m_localPlayerContext = LocalPlayerContext{};
    }

    m_input.clearMovePreview();
    m_input.clearSelection();
    m_input.setTool(ToolState::Select);
    m_engine.resetPendingTurn();
    invalidateTurnDraft();
    m_uiManager.hideGameMenu();
    m_uiManager.hideMultiplayerAlert();
    m_uiManager.hideMultiplayerWaitingOverlay();
    m_uiManager.clearMultiplayerStatus();
}

bool Game::startMultiplayerHostIfNeeded(const GameSessionConfig& session, std::string* errorMessage) {
    m_lanHostRemoteSessionEstablished = false;

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

    const CheckTurnValidation validation = validateActivePendingTurn();
    if (!validation.valid) {
        if (errorMessage) {
            *errorMessage = validation.errorMessage;
        }
        return false;
    }

    if (!m_multiplayerClient.sendTurnSubmission(turnSystem().getPendingCommands(), errorMessage)) {
        return false;
    }

    const InputSelectionBookmark selectionBookmark = captureSelectionBookmark();
    m_waitingForRemoteTurnResult = true;
    m_input.clearMovePreview();
    turnSystem().resetPendingCommands();
    ensureTurnDraftUpToDate();
    reconcileSelectionBookmark(selectionBookmark);
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
    for (const auto& submittedCommand : commands) {
        TurnCommand command = submittedCommand;
        if (command.type == TurnCommand::Build) {
            // Host-side builds must reserve authoritative ids locally.
            command.buildId = -1;
        }

        if (!turnSystem().queueCommand(command,
                                       board(),
                                       activeKingdom(),
                                       enemyKingdom(),
                                       publicBuildings(),
                                       m_config,
                                       &buildingFactory())) {
            turnSystem().resetPendingCommands();
            if (errorMessage) {
                *errorMessage = "The remote player submitted an invalid or conflicting turn command.";
            }
            return false;
        }
    }

    const CheckTurnValidation validation = validateActivePendingTurn();
    if (!validation.valid) {
        if (validation.activeKingInCheck && !validation.hasAnyLegalResponse) {
            commitAuthoritativeTurn();
        } else {
            turnSystem().resetPendingCommands();
        }
        if (errorMessage) {
            *errorMessage = validation.errorMessage;
        }
        return false;
    }

    commitAuthoritativeTurn();
    return true;
}

bool Game::joinMultiplayer(const JoinMultiplayerRequest& request, std::string* errorMessage) {
    clearReconnectState();
    return joinMultiplayerInternal(request, false, errorMessage);
}

bool Game::reconnectToMultiplayerHost(std::string* errorMessage) {
    if (!m_clientReconnectState.available) {
        writeMultiplayerError(errorMessage, "No previous multiplayer host is available for reconnect.");
        return false;
    }

    return joinMultiplayerInternal(m_clientReconnectState.request, true, errorMessage);
}

bool Game::joinMultiplayerInternal(const JoinMultiplayerRequest& request,
                                  bool preserveLanClientContext,
                                  std::string* errorMessage) {
    const InputSelectionBookmark selectionBookmark = captureSelectionBookmark();
    discardPendingAITurn();
    prepareForClientConnectionAttempt(preserveLanClientContext);

    const sf::IpAddress address(request.host);
    if (address == sf::IpAddress::None) {
        writeMultiplayerError(errorMessage, "Invalid server IP address.");
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
    if (!waitForServerInfo(m_multiplayerClient, serverInfo, sf::seconds(3.f), errorMessage)) {
        return false;
    }
    if (!serverInfo.multiplayerEnabled) {
        writeMultiplayerError(errorMessage, "This server is not hosting a multiplayer save.");
        m_multiplayerClient.disconnect();
        return false;
    }
    if (serverInfo.protocolVersion != kCurrentMultiplayerProtocolVersion) {
        writeMultiplayerError(errorMessage, "The multiplayer host uses an incompatible protocol version.");
        m_multiplayerClient.disconnect();
        return false;
    }
    if (!serverInfo.joinable) {
        writeMultiplayerError(errorMessage, "The multiplayer server is already occupied.");
        m_multiplayerClient.disconnect();
        return false;
    }

    const std::string digest = MultiplayerPasswordUtils::computePasswordDigest(
        request.password, serverInfo.passwordSalt);
    if (!m_multiplayerClient.sendJoinRequest(digest, errorMessage)) {
        m_multiplayerClient.disconnect();
        return false;
    }

    SaveData snapshotData;
    if (!waitForJoinSnapshot(m_multiplayerClient, m_saveManager, snapshotData, sf::seconds(5.f), errorMessage)) {
        return false;
    }

    std::string restoreError;
    if (!m_engine.restoreFromSave(snapshotData, m_config, &restoreError)) {
        writeMultiplayerError(
            errorMessage,
            restoreError.empty() ? "Unable to restore the multiplayer snapshot." : restoreError);
        m_multiplayerClient.disconnect();
        return false;
    }

    cacheReconnectRequest(request);
    m_localPlayerContext = makeLanClientLocalPlayerContext();
    m_waitingForRemoteTurnResult = false;
    m_clientReconnectState.reconnectAttemptInProgress = false;
    m_state = GameState::Playing;
    m_input.clearMovePreview();
    refreshTurnPhase();
    reconcileSelectionBookmark(selectionBookmark);
    centerCameraOnKingdom(KingdomId::Black);
    m_uiManager.hideMultiplayerAlert();
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
        case MultiplayerServer::Event::Type::ClientConnected: {
            m_uiManager.hideMultiplayerAlert();
            eventLog().log(turnSystem().getTurnNumber(), KingdomId::Black, event.message);
            m_lanHostRemoteSessionEstablished = false;
            std::string snapshotError;
            if (!pushSnapshotToRemote(&snapshotError)) {
                if (!snapshotError.empty()) {
                    eventLog().log(turnSystem().getTurnNumber(), KingdomId::Black, snapshotError);
                }
            } else {
                m_lanHostRemoteSessionEstablished = true;
            }
            break;
        }

        case MultiplayerServer::Event::Type::ClientDisconnected: {
            const bool wasEstablished = m_lanHostRemoteSessionEstablished;
            m_lanHostRemoteSessionEstablished = false;
            eventLog().log(turnSystem().getTurnNumber(), KingdomId::Black, event.message);
            if (wasEstablished) {
                m_uiManager.showMultiplayerAlert(
                    "Black Disconnected",
                    event.message + "\n\nWhite cannot continue until Black reconnects.",
                    "Continue");
            }
            break;
        }

        case MultiplayerServer::Event::Type::ClientConnectionInterrupted:
            m_lanHostRemoteSessionEstablished = false;
            if (!event.message.empty()) {
                eventLog().log(turnSystem().getTurnNumber(), KingdomId::Black, event.message);
            }
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
            const InputSelectionBookmark selectionBookmark = captureSelectionBookmark();
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
            m_clientReconnectState.awaitingReconnect = false;
            m_clientReconnectState.reconnectAttemptInProgress = false;
            m_clientReconnectState.lastErrorMessage.clear();
            m_input.clearMovePreview();
            m_uiManager.hideMultiplayerAlert();
            refreshTurnPhase();
            reconcileSelectionBookmark(selectionBookmark);
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
            m_multiplayerClient.disconnect();
            m_localPlayerContext = makeLanClientLocalPlayerContext();
            m_waitingForRemoteTurnResult = false;
            m_clientReconnectState.reconnectAttemptInProgress = false;
            m_input.clearMovePreview();
            m_input.clearSelection();
            m_input.setTool(ToolState::Select);
            m_engine.resetPendingTurn();
            invalidateTurnDraft();
            showLanClientDisconnectAlert(
                event.type == MultiplayerClient::Event::Type::Disconnected ? "Host Disconnected" : "Network Error",
                message);
            break;
        }

        default:
            break;
    }
}

void Game::showLanClientDisconnectAlert(const std::string& title, const std::string& message) {
    m_clientReconnectState.awaitingReconnect = true;
    m_clientReconnectState.reconnectAttemptInProgress = false;
    m_clientReconnectState.lastErrorMessage = message;
    m_uiManager.hideGameMenu();

    if (!m_clientReconnectState.available) {
        m_uiManager.showMultiplayerAlert(
            title,
            message,
            "Return to Main Menu",
            [this]() {
                returnToMainMenu();
            });
        return;
    }

    m_uiManager.showMultiplayerAlert(
        title,
        message,
        MultiplayerDialogAction{
            "Reconnect",
            [this]() {
                if (m_clientReconnectState.reconnectAttemptInProgress) {
                    return;
                }

                m_clientReconnectState.reconnectAttemptInProgress = true;
                std::string error;
                if (!reconnectToMultiplayerHost(&error)) {
                    m_clientReconnectState.reconnectAttemptInProgress = false;
                    const std::string reconnectMessage = error.empty()
                        ? "Unable to reconnect to the previous multiplayer host."
                        : "Unable to reconnect to the previous multiplayer host.\n\n" + error;
                    showLanClientDisconnectAlert("Reconnect Failed", reconnectMessage);
                }
            }},
        MultiplayerDialogAction{
            "Return to Main Menu",
            [this]() {
                returnToMainMenu();
            }});
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
        if (m_state != GameState::Playing && m_state != GameState::Paused && m_state != GameState::GameOver) {
            return;
        }

        toggleInGameMenu();
    });
    m_uiManager.hud().setOnResetTurn([this]() {
        if (!currentInteractionPermissions().canIssueCommands) return;
        resetPlayerTurn();
    });
    m_uiManager.hud().setOnEndTurn([this]() {
        if (!currentInteractionPermissions().canIssueCommands) return;
        commitPlayerTurn();
    });

    // Toolbar
    m_uiManager.toolBar().setOnSelect([this]() {
        if (!currentInteractionPermissions().canUseToolbar) return;
        activateSelectTool();
    });
    m_uiManager.toolBar().setOnBuild([this]() {
        const InteractionPermissions permissions = currentInteractionPermissions();
        if (!permissions.canUseToolbar || !permissions.canOpenBuildPanel) return;
        m_input.setTool(ToolState::Build);
        m_uiManager.buildToolPanel().setSelectedBuildType(m_input.getBuildPreviewType());
        const KingdomId viewedKingdomId = permissions.canIssueCommands
            ? activeKingdom().id
            : localPerspectiveKingdom();
        ensureTurnDraftUpToDate();
        m_uiManager.showBuildToolPanel(displayedKingdom(viewedKingdomId),
                                       m_config,
                                       permissions.canQueueNonMoveActions);
    });
    m_uiManager.toolBar().setOnOverview([this]() {
        if (!currentInteractionPermissions().canUseToolbar) return;
        m_uiManager.toggleRightSidebar();
    });

    // Build tool panel
    m_uiManager.buildToolPanel().setOnSelectBuildType([this](int type) {
        if (!currentInteractionPermissions().canQueueNonMoveActions) return;
        const BuildingType buildingType = static_cast<BuildingType>(type);
        m_input.setBuildType(buildingType);
        m_uiManager.buildToolPanel().setSelectedBuildType(buildingType);
    });

    m_uiManager.buildingPanel().setOnCancelConstruction([this](int buildingId) {
        if (!currentInteractionPermissions().canQueueNonMoveActions) return;
        if (turnSystem().cancelBuildCommand(buildingId,
                                            board(),
                                            activeKingdom(),
                                            enemyKingdom(),
                                            publicBuildings(),
                                            m_config)) {
            m_input.clearSelection();
        }
    });

    m_uiManager.barracksPanel().setOnCancelConstruction([this](int buildingId) {
        if (!currentInteractionPermissions().canQueueNonMoveActions) return;
        if (turnSystem().cancelBuildCommand(buildingId,
                                            board(),
                                            activeKingdom(),
                                            enemyKingdom(),
                                            publicBuildings(),
                                            m_config)) {
            m_input.clearSelection();
        }
    });

    // Piece panel upgrade
    m_uiManager.piecePanel().setOnUpgrade([this](int pieceId, int targetType) {
        if (!currentInteractionPermissions().canQueueNonMoveActions) return;
        Piece* piece = activeKingdom().getPieceById(pieceId);
        if (!piece) return;
        const TurnCommand* queuedUpgrade = turnSystem().getPendingUpgradeCommand(pieceId);
        const PieceType requestedTarget = static_cast<PieceType>(targetType);
        TurnCommand previousUpgrade;
        const bool hadQueuedUpgrade = (queuedUpgrade != nullptr);
        const bool cancelOnly = hadQueuedUpgrade && queuedUpgrade->upgradeTarget == requestedTarget;
        if (hadQueuedUpgrade) {
            previousUpgrade = *queuedUpgrade;
            if (!turnSystem().cancelUpgradeCommand(pieceId,
                                                   board(),
                                                   activeKingdom(),
                                                   enemyKingdom(),
                                                   publicBuildings(),
                                                   m_config)) {
                return;
            }
            if (cancelOnly) {
                return;
            }
        }

        TurnCommand cmd;
        cmd.type = TurnCommand::Upgrade;
        cmd.upgradePieceId = piece->id;
        cmd.upgradeTarget = requestedTarget;
        if (!piece->canUpgradeTo(cmd.upgradeTarget, m_config)) {
            if (hadQueuedUpgrade) {
                turnSystem().queueCommand(previousUpgrade,
                                          board(),
                                          activeKingdom(),
                                          enemyKingdom(),
                                          publicBuildings(),
                                          m_config);
            }
            return;
        }
        if (!turnSystem().queueCommand(cmd,
                                       board(),
                                       activeKingdom(),
                                       enemyKingdom(),
                                       publicBuildings(),
                                           m_config,
                                           &buildingFactory())
            && hadQueuedUpgrade) {
            turnSystem().queueCommand(previousUpgrade,
                                      board(),
                                      activeKingdom(),
                                      enemyKingdom(),
                                      publicBuildings(),
                                      m_config);
        }
    });

    m_uiManager.piecePanel().setOnDisband([this](int pieceId) {
        if (!currentInteractionPermissions().canQueueNonMoveActions) return;
        Piece* piece = activeKingdom().getPieceById(pieceId);
        if (!piece || piece->type == PieceType::King) return;

        if (turnSystem().getPendingDisbandCommand(pieceId)) {
            turnSystem().cancelDisbandCommand(pieceId,
                                              board(),
                                              activeKingdom(),
                                              enemyKingdom(),
                                              publicBuildings(),
                                              m_config);
            return;
        }

        TurnCommand cmd;
        cmd.type = TurnCommand::Disband;
        cmd.pieceId = pieceId;
        turnSystem().queueCommand(cmd,
                                  board(),
                                  activeKingdom(),
                                  enemyKingdom(),
                                  publicBuildings(),
                                  m_config,
                                  &buildingFactory());
    });

    // Barracks panel produce
    m_uiManager.barracksPanel().setOnProduce([this](int barracksId, int pieceType) {
        if (!currentInteractionPermissions().canQueueNonMoveActions) return;
        const TurnCommand* queuedProduction = turnSystem().getPendingProduceCommand(barracksId);
        const PieceType requestedType = static_cast<PieceType>(pieceType);
        TurnCommand previousProduction;
        const bool hadQueuedProduction = (queuedProduction != nullptr);
        const bool cancelOnly = hadQueuedProduction && queuedProduction->produceType == requestedType;
        if (hadQueuedProduction) {
            previousProduction = *queuedProduction;
            if (!turnSystem().cancelProduceCommand(barracksId,
                                                   board(),
                                                   activeKingdom(),
                                                   enemyKingdom(),
                                                   publicBuildings(),
                                                   m_config)) {
                return;
            }
            if (cancelOnly) {
                return;
            }
        }

        TurnCommand cmd;
        cmd.type = TurnCommand::Produce;
        cmd.barracksId = barracksId;
        cmd.produceType = requestedType;
        if (!turnSystem().queueCommand(cmd,
                                       board(),
                                       activeKingdom(),
                                       enemyKingdom(),
                                       publicBuildings(),
                                       m_config,
                                       &buildingFactory())
            && hadQueuedProduction) {
            turnSystem().queueCommand(previousProduction,
                                      board(),
                                      activeKingdom(),
                                      enemyKingdom(),
                                      publicBuildings(),
                                      m_config);
        }
    });
}

void Game::updateUIState() {
    ensureTurnDraftUpToDate();
    turnSystem().syncPointBudget(m_config);
    const CheckTurnValidation validation = validateActivePendingTurn();
    const InteractionPermissions permissions = currentInteractionPermissions(&validation);
    const InGameHudPresentation hudPresentation = buildInGameHudPresentation();
    InGameViewModel viewModel = buildInGameViewModel(
        m_engine,
        m_config,
        m_state,
        permissions.canIssueCommands,
        hudPresentation);
    if (shouldUseTurnDraft() && m_turnDraft.isValid()) {
        const Kingdom& displayedHudKingdom = displayedKingdom(hudPresentation.statsKingdom);
        const Kingdom& displayedWhiteKingdom = displayedKingdom(KingdomId::White);
        const Kingdom& displayedBlackKingdom = displayedKingdom(KingdomId::Black);

        viewModel.activeGold = displayedHudKingdom.gold;
        viewModel.activeOccupiedCells = countDisplayedOccupiedBuildingCells(displayedHudKingdom);
        viewModel.activeTroops = displayedHudKingdom.pieceCount();
        viewModel.activeIncome = EconomySystem::calculateProjectedNetIncome(
            displayedHudKingdom,
            displayedBoard(),
            displayedPublicBuildings(),
            m_config);
        viewModel.balanceMetrics[0].whiteValue = displayedWhiteKingdom.gold;
        viewModel.balanceMetrics[0].blackValue = displayedBlackKingdom.gold;
        viewModel.balanceMetrics[1].whiteValue = countDisplayedOccupiedBuildingCells(displayedWhiteKingdom);
        viewModel.balanceMetrics[1].blackValue = countDisplayedOccupiedBuildingCells(displayedBlackKingdom);
        viewModel.balanceMetrics[3].whiteValue = EconomySystem::calculateProjectedNetIncome(
            displayedWhiteKingdom,
            displayedBoard(),
            displayedPublicBuildings(),
            m_config);
        viewModel.balanceMetrics[3].blackValue = EconomySystem::calculateProjectedNetIncome(
            displayedBlackKingdom,
            displayedBoard(),
            displayedPublicBuildings(),
            m_config);
    }

    viewModel.alerts.clear();
    if (m_state == GameState::GameOver && validation.activeKingInCheck && !validation.hasAnyLegalResponse) {
        const KingdomId winner = opponent(activeKingdom().id);
        const bool localHotseat = m_localPlayerContext.mode == LocalSessionMode::LocalOnly
            && m_localPlayerContext.localControl[kingdomIndex(KingdomId::White)]
            && m_localPlayerContext.localControl[kingdomIndex(KingdomId::Black)];
        if (localHotseat) {
            viewModel.alerts.push_back({
                "Checkmate - " + std::string(kingdomName(winner)) + " wins",
                InGameAlertTone::Danger
            });
        } else {
            const bool localWon = localPerspectiveKingdom() == winner;
            viewModel.alerts.push_back({
                localWon ? "Checkmate - You Win" : "Checkmate - You Lose",
                localWon ? InGameAlertTone::Success : InGameAlertTone::Danger
            });
        }
    } else {
        if (validation.activeKingInCheck) {
            viewModel.alerts.push_back({"Check", InGameAlertTone::Danger});
        }
        if (validation.bankrupt) {
            viewModel.alerts.push_back({
                "Bankruptcy: end turn would reach " + std::to_string(validation.projectedEndingGold) + " gold.",
                InGameAlertTone::Warning
            });
        }
    }

    viewModel.canEndTurn = permissions.canIssueCommands && validation.valid;
    m_uiManager.updateDashboard(viewModel);
    m_uiManager.toolBar().applyPresentation(makeToolBarPresentation(
        m_input.getCurrentTool(),
        permissions.canUseToolbar,
        permissions.canOpenBuildPanel,
        m_uiManager.isRightSidebarVisible()));
    updateMultiplayerPresentation();
    const KingdomId viewedKingdomId = permissions.canIssueCommands ? activeKingdom().id : localPerspectiveKingdom();

    switch (m_input.getCurrentTool()) {
        case ToolState::Build:
            if (permissions.canOpenBuildPanel) {
                m_uiManager.showBuildToolPanel(displayedKingdom(viewedKingdomId),
                                               m_config,
                                               permissions.canQueueNonMoveActions);
            } else {
                m_uiManager.showSelectionEmptyState();
            }
            break;

        case ToolState::Select:
        default:
            if (m_input.getSelectedPiece()) {
                const Piece& selectedPiece = *m_input.getSelectedPiece();
                const bool ownsSelectedPiece = selectedPiece.kingdom == viewedKingdomId;
                const TurnCommand* pendingUpgrade = turnSystem().getPendingUpgradeCommand(selectedPiece.id);
                const TurnCommand* pendingDisband = turnSystem().getPendingDisbandCommand(selectedPiece.id);
                const bool allowUpgrade = permissions.canQueueNonMoveActions
                    && ownsSelectedPiece
                    && pendingDisband == nullptr;
                const bool allowDisband = permissions.canQueueNonMoveActions
                    && ownsSelectedPiece
                    && selectedPiece.type != PieceType::King
                    && (pendingDisband != nullptr
                        || (!turnSystem().hasPendingMoveForPiece(selectedPiece.id)
                            && pendingUpgrade == nullptr));
                m_uiManager.showPiecePanel(*m_input.getSelectedPiece(),
                                           m_config,
                                           allowUpgrade,
                                           allowDisband,
                                           pendingUpgrade,
                                           pendingDisband);
            } else if (m_input.getSelectedBuilding()) {
                Building* building = m_input.getSelectedBuilding();
                const bool allowCancelConstruction = permissions.canQueueNonMoveActions
                    && building->isUnderConstruction()
                    && building->owner == turnSystem().getActiveKingdom();
                if (building->type == BuildingType::Barracks) {
                    const bool allowProduce = permissions.canQueueNonMoveActions
                        && !building->isUnderConstruction()
                        && (building->owner == viewedKingdomId);
                    const TurnCommand* pendingProduce = turnSystem().getPendingProduceCommand(building->id);
                    m_uiManager.showBarracksPanel(*building,
                                                  displayedKingdom(building->owner),
                                                  m_config,
                                                  allowProduce,
                                                  allowCancelConstruction,
                                                  pendingProduce);
                } else {
                    std::optional<ResourceIncomeBreakdown> resourceIncome;
                    std::optional<PublicBuildingOccupationState> publicOccupation;
                    if (building->hasActiveGameplayEffects()
                        && (building->type == BuildingType::Mine || building->type == BuildingType::Farm)) {
                        resourceIncome = EconomySystem::calculateResourceIncomeBreakdown(
                            *building,
                            displayedBoard(),
                            m_config);
                    }
                    if (building->isPublic()) {
                        publicOccupation = resolvePublicBuildingOccupationState(*building, displayedBoard());
                    }
                    m_uiManager.showBuildingPanel(*building,
                                                  allowCancelConstruction,
                                                  resourceIncome,
                                                  publicOccupation);
                }
            } else if (m_input.hasSelectedCell()) {
                const sf::Vector2i cellPos = m_input.getSelectedCell();
                m_uiManager.showCellPanel(displayedBoard().getCell(cellPos.x, cellPos.y));
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
        const bool restrictToResponseMove = isActiveKingInCheckForRules();
        for (const auto& cmd : directorPlan.commands) {
            if (restrictToResponseMove && cmd.type != TurnCommand::Move) {
                continue;
            }
            turnSystem().queueCommand(cmd,
                                      board(),
                                      activeKingdom(),
                                      enemyKingdom(),
                                      publicBuildings(),
                                      m_config,
                                      &buildingFactory());
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
        const bool restrictToResponseMove = isActiveKingInCheckForRules();
        for (const auto& cmd : plan.commands) {
            if (restrictToResponseMove && cmd.type != TurnCommand::Move) {
                continue;
            }
            turnSystem().queueCommand(cmd,
                                      board(),
                                      activeKingdom(),
                                      enemyKingdom(),
                                      publicBuildings(),
                                      m_config,
                                      &buildingFactory());
        }
    }
    eventLog().log(task->turnNumber, task->activeKingdom, "AI completed turn planning.");

    CheckTurnValidation aiValidation = validateActivePendingTurn();
    if (!aiValidation.valid && aiValidation.hasAnyLegalResponse) {
        turnSystem().resetPendingCommands();

        if (aiValidation.activeKingInCheck) {
            for (Piece& piece : activeKingdom().pieces) {
                const std::vector<sf::Vector2i> legalMoves = CheckResponseRules::filterLegalMovesForPiece(
                    piece, board(), m_config);
                if (legalMoves.empty()) {
                    continue;
                }

                TurnCommand fallbackMove;
                fallbackMove.type = TurnCommand::Move;
                fallbackMove.pieceId = piece.id;
                fallbackMove.origin = piece.position;
                fallbackMove.destination = legalMoves.front();
                turnSystem().queueCommand(fallbackMove,
                                          board(),
                                          activeKingdom(),
                                          enemyKingdom(),
                                          publicBuildings(),
                                          m_config,
                                          &buildingFactory());
                break;
            }
        }

        aiValidation = validateActivePendingTurn();
        if (aiValidation.bankrupt) {
            std::vector<int> disbandCandidates;
            disbandCandidates.reserve(activeKingdom().pieces.size());
            for (const Piece& piece : activeKingdom().pieces) {
                if (piece.type == PieceType::King) {
                    continue;
                }

                disbandCandidates.push_back(piece.id);
            }

            std::sort(disbandCandidates.begin(), disbandCandidates.end(), [this](int lhsId, int rhsId) {
                const Piece* lhs = activeKingdom().getPieceById(lhsId);
                const Piece* rhs = activeKingdom().getPieceById(rhsId);
                const int lhsUpkeep = lhs ? m_config.getPieceUpkeepCost(lhs->type) : 0;
                const int rhsUpkeep = rhs ? m_config.getPieceUpkeepCost(rhs->type) : 0;
                if (lhsUpkeep != rhsUpkeep) {
                    return lhsUpkeep > rhsUpkeep;
                }

                return lhsId < rhsId;
            });

            for (const int pieceId : disbandCandidates) {
                TurnCommand disbandCommand;
                disbandCommand.type = TurnCommand::Disband;
                disbandCommand.pieceId = pieceId;
                if (!turnSystem().queueCommand(disbandCommand,
                                               board(),
                                               activeKingdom(),
                                               enemyKingdom(),
                                               publicBuildings(),
                                               m_config,
                                               &buildingFactory())) {
                    continue;
                }

                aiValidation = validateActivePendingTurn();
                if (aiValidation.valid) {
                    break;
                }
            }
        }
    }

    commitAuthoritativeTurn();
}
