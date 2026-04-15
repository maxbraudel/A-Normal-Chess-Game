#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <SFML/System/Time.hpp>

#include "AI/AIBrain.hpp"
#include "AI/AIStrategySpecial.hpp"
#include "Board/Board.hpp"
#include "Board/BoardGenerator.hpp"
#include "Board/CellType.hpp"
#include "Buildings/BuildingFactory.hpp"
#include "Buildings/StructureChunkRegistry.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/AIConfig.hpp"
#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Core/LocalPlayerContext.hpp"
#include "Core/GameState.hpp"
#include "Core/GameStateValidator.hpp"
#include "Input/LayeredSelection.hpp"
#include "Render/StructureOverlay.hpp"
#include "Multiplayer/MultiplayerClient.hpp"
#include "Multiplayer/PasswordUtils.hpp"
#include "Multiplayer/Protocol.hpp"
#include "Multiplayer/MultiplayerServer.hpp"
#include "Save/SaveManager.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/TurnCommand.hpp"
#include "Systems/TurnSystem.hpp"
#include "UI/InGameViewModelBuilder.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceFactory.hpp"

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

Building makeTestBarracks(int id, KingdomId owner, const sf::Vector2i& origin, const GameConfig& config) {
    Building barracks;
    barracks.id = id;
    barracks.type = BuildingType::Barracks;
    barracks.owner = owner;
    barracks.isNeutral = false;
    barracks.origin = origin;
    barracks.width = config.getBuildingWidth(BuildingType::Barracks);
    barracks.height = config.getBuildingHeight(BuildingType::Barracks);
    barracks.cellHP.assign(barracks.width * barracks.height, config.getBarracksCellHP());
    return barracks;
}

Building makeTestPublicBuilding(BuildingType type, const sf::Vector2i& origin, int width, int height) {
    Building building;
    building.type = type;
    building.isNeutral = true;
    building.origin = origin;
    building.width = width;
    building.height = height;
    building.cellHP.assign(width * height, 1);
    return building;
}

template <typename Predicate>
bool waitUntil(Predicate predicate, int timeoutMs = 1000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return predicate();
}

unsigned short startLoopbackServerOnFreePort(MultiplayerServer& server,
                                            const MultiplayerConfig& config,
                                            const std::string& saveName,
                                            unsigned short firstPort,
                                            unsigned short lastPort,
                                            const std::string& failureMessage) {
    for (unsigned short port = firstPort; port < lastPort; ++port) {
        std::string startError;
        if (server.start(port, saveName, config, &startError)) {
            return port;
        }
    }

    throw std::runtime_error(failureMessage);
}

void connectAndAuthenticateLoopbackClient(MultiplayerServer& server,
                                          MultiplayerClient& client,
                                          unsigned short port,
                                          const std::string& password,
                                          const std::string& scenarioLabel) {
    std::string error;
    expect(client.connect(sf::IpAddress::LocalHost, port, sf::seconds(1.f), &error),
           scenarioLabel + " client should connect to the local server.");
    expect(client.requestServerInfo(&error),
           scenarioLabel + " client should request server info.");

    MultiplayerServerInfo info;
    expect(waitUntil([&]() {
        server.update();
        client.update();

        while (client.hasPendingEvent()) {
            const auto event = client.popNextEvent();
            if (event.type == MultiplayerClient::Event::Type::ServerInfoReceived) {
                info = event.serverInfo;
                return true;
            }
        }

        return false;
    }), scenarioLabel + " should receive server info.");

    expect(info.joinable, scenarioLabel + " server should be joinable.");
    expect(client.sendJoinRequest(MultiplayerPasswordUtils::computePasswordDigest(password, info.passwordSalt), &error),
           scenarioLabel + " client should send a join request.");

    bool joinAccepted = false;
    bool serverConnectedEvent = false;
    bool unexpectedDisconnect = false;
    expect(waitUntil([&]() {
        server.update();
        client.update();

        while (server.hasPendingEvent()) {
            const auto event = server.popNextEvent();
            if (event.type == MultiplayerServer::Event::Type::ClientConnected) {
                serverConnectedEvent = true;
            }
            if (event.type == MultiplayerServer::Event::Type::ClientDisconnected
                || event.type == MultiplayerServer::Event::Type::ClientConnectionInterrupted) {
                unexpectedDisconnect = true;
            }
        }

        while (client.hasPendingEvent()) {
            const auto event = client.popNextEvent();
            if (event.type == MultiplayerClient::Event::Type::JoinAccepted) {
                joinAccepted = true;
            }
            if (event.type == MultiplayerClient::Event::Type::JoinRejected
                || event.type == MultiplayerClient::Event::Type::Disconnected
                || event.type == MultiplayerClient::Event::Type::Error) {
                unexpectedDisconnect = true;
            }
        }

        return joinAccepted && serverConnectedEvent;
    }), scenarioLabel + " join handshake should complete.");

    expect(!unexpectedDisconnect, scenarioLabel + " should not drop during the join handshake.");
}

sf::Vector2i findEmptyTraversableCell(const GameEngine& engine) {
    const Board& board = engine.board();
    const int diameter = board.getDiameter();
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle || cell.type == CellType::Water || cell.type == CellType::Void) {
                continue;
            }
            if (cell.piece == nullptr && cell.building == nullptr) {
                return {x, y};
            }
        }
    }

    throw std::runtime_error("No empty traversable cell found for test setup.");
}

std::string buildGenerationSignature(const Board& board,
                                    const std::vector<Building>& publicBuildings,
                                    const GenerationResult& generation) {
    std::string signature;
    const int diameter = board.getDiameter();
    signature.reserve(diameter * diameter * 3);

    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle) {
                continue;
            }

            switch (cell.type) {
                case CellType::Grass: signature.push_back('G'); break;
                case CellType::Dirt: signature.push_back('D'); break;
                case CellType::Water: signature.push_back('W'); break;
                case CellType::Void: signature.push_back('V'); break;
            }

            signature += std::to_string(cell.terrainFlipMask);
            signature.push_back(',');
        }
    }

    signature += '|';
    for (const auto& building : publicBuildings) {
        signature += std::to_string(static_cast<int>(building.type));
        signature += ':';
        signature += std::to_string(building.origin.x);
        signature += ',';
        signature += std::to_string(building.origin.y);
        signature += ',';
        signature += std::to_string(building.rotationQuarterTurns);
        signature += ',';
        signature += std::to_string(building.flipMask);
        signature += ';';
    }

    signature += '|';
    signature += std::to_string(generation.playerSpawn.x);
    signature += ',';
    signature += std::to_string(generation.playerSpawn.y);
    signature += '|';
    signature += std::to_string(generation.aiSpawn.x);
    signature += ',';
    signature += std::to_string(generation.aiSpawn.y);
    return signature;
}

int largestConnectedRegion(const Board& board, CellType type) {
    const int diameter = board.getDiameter();
    std::vector<std::vector<bool>> visited(diameter, std::vector<bool>(diameter, false));
    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};
    int best = 0;

    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            if (visited[y][x]) {
                continue;
            }

            const Cell& start = board.getCell(x, y);
            if (!start.isInCircle || start.type != type) {
                continue;
            }

            int size = 0;
            std::queue<sf::Vector2i> frontier;
            frontier.push({x, y});
            visited[y][x] = true;

            while (!frontier.empty()) {
                const sf::Vector2i current = frontier.front();
                frontier.pop();
                ++size;

                for (int direction = 0; direction < 4; ++direction) {
                    const int nx = current.x + dx[direction];
                    const int ny = current.y + dy[direction];
                    if (nx < 0 || ny < 0 || nx >= diameter || ny >= diameter || visited[ny][nx]) {
                        continue;
                    }

                    const Cell& next = board.getCell(nx, ny);
                    if (!next.isInCircle || next.type != type) {
                        continue;
                    }

                    visited[ny][nx] = true;
                    frontier.push({nx, ny});
                }
            }

            best = std::max(best, size);
        }
    }

    return best;
}

void expectPublicBuildingsAvoidWater(const Board& board, const std::vector<Building>& publicBuildings) {
    for (const auto& building : publicBuildings) {
        for (const auto& occupied : building.getOccupiedCells()) {
            expect(board.getCell(occupied.x, occupied.y).type != CellType::Water,
                   "Public buildings must not overlap water cells.");
        }
    }
}

void testLayeredSelectionStackResolvesPriority() {
    Cell cell;
    cell.type = CellType::Grass;
    cell.isInCircle = true;

    Building building;
    building.type = BuildingType::Barracks;
    cell.building = &building;

    Piece piece(11, PieceType::Knight, KingdomId::White, {4, 5});
    cell.piece = &piece;

    const LayeredSelectionStack stack = resolveCellSelectionStack(cell, {4, 5});
    expect(stack.count == 3, "Stacked cells should expose piece, building, and terrain layers.");
    expect(stack.top() == SelectionLayer::Piece,
        "Piece should remain the top selection layer when all three layers exist.");
    expect(stack.nextBelow(SelectionLayer::Piece) == SelectionLayer::Building,
        "Cycling down from a piece should expose the underlying building next.");
    expect(stack.nextBelow(SelectionLayer::Building) == SelectionLayer::Terrain,
        "Cycling down from a building should expose terrain next.");
    expect(stack.nextBelow(SelectionLayer::Terrain) == SelectionLayer::Piece,
        "Cycling down from terrain should wrap back to the topmost piece layer.");
}

void testLayeredSelectionStackSupportsBuildingTerrainCycle() {
    Cell cell;
    cell.type = CellType::Dirt;
    cell.isInCircle = true;

    Building building;
    building.type = BuildingType::Farm;
    cell.building = &building;

    const LayeredSelectionStack stack = resolveCellSelectionStack(cell, {2, 3});
    expect(stack.count == 2, "Building cells without a piece should expose building plus terrain only.");
    expect(stack.top() == SelectionLayer::Building,
        "Building should be the top layer when no piece is present.");
    expect(stack.nextBelow(SelectionLayer::Building) == SelectionLayer::Terrain,
        "Cycling down from a building-only stack should reach terrain.");
    expect(stack.nextBelow(SelectionLayer::Terrain) == SelectionLayer::Building,
        "Terrain should wrap back to building when those are the only two layers.");
}

void testLayeredSelectionStackSupportsPreviewPieceOverride() {
    Cell cell;
    cell.type = CellType::Grass;
    cell.isInCircle = true;

    Building building;
    building.type = BuildingType::Mine;
    cell.building = &building;

    Piece hiddenPiece(21, PieceType::Pawn, KingdomId::Black, {6, 6});
    cell.piece = &hiddenPiece;
    Piece previewPiece(22, PieceType::Rook, KingdomId::White, {6, 6});

    const LayeredSelectionStack stack = resolveCellSelectionStack(cell, {6, 6}, &previewPiece, true);
    expect(stack.piece == &previewPiece,
        "Preview resolution should let the visually moved piece override the cell piece pointer.");
    expect(stack.top() == SelectionLayer::Piece && stack.count == 3,
        "Preview overrides should still preserve the standard piece > building > terrain stack.");
}

    void testPublicBuildingOccupationStateResolvesAllOutcomes() {
        Board board;
        board.init(5);

        Building mine = makeTestPublicBuilding(BuildingType::Mine, {1, 1}, 2, 1);
        Kingdom white(KingdomId::White);
        white.addPiece(Piece(31, PieceType::Pawn, KingdomId::White, {1, 1}));
        Kingdom black(KingdomId::Black);
        black.addPiece(Piece(32, PieceType::Pawn, KingdomId::Black, {2, 1}));

        expect(resolvePublicBuildingOccupationState(mine, board) == PublicBuildingOccupationState::Unoccupied,
            "Unoccupied public buildings should resolve to the empty occupation state.");

        board.getCell(1, 1).piece = &white.pieces.back();
        expect(resolvePublicBuildingOccupationState(mine, board) == PublicBuildingOccupationState::WhiteOccupied,
            "White-only presence should resolve to the white occupation state.");

        board.getCell(1, 1).piece = nullptr;
        board.getCell(2, 1).piece = &black.pieces.back();
        expect(resolvePublicBuildingOccupationState(mine, board) == PublicBuildingOccupationState::BlackOccupied,
            "Black-only presence should resolve to the black occupation state.");

        board.getCell(1, 1).piece = &white.pieces.back();
        expect(resolvePublicBuildingOccupationState(mine, board) == PublicBuildingOccupationState::Contested,
            "Mixed kingdom presence should resolve to the contested occupation state.");
    }

    void testSelectedStructureOverlayPrivateBuildingsUseOwnerShield() {
        GameConfig config;
        Board board;
        board.init(5);

        const Building barracks = makeTestBarracks(7, KingdomId::Black, {1, 1}, config);
        StructureOverlayContext overlayContext;
        overlayContext.isSelected = false;
        const StructureOverlayStack overlay = buildStructureOverlay(
            barracks, board, config, overlayContext, makeWorldStructureOverlayPolicy());

        expect(overlay.rows.size() == 1,
            "Idle private buildings should expose a single status row.");
        expect(overlay.rows[0].placement == StructureOverlayRowPlacement::Above,
            "The private building status row should be the primary row above the structure.");
        expect(overlay.rows[0].items.size() == 1,
            "The private building status row should currently contain a single owner icon.");
        expect(overlay.rows[0].items[0].type == StructureOverlayItemType::Icon,
            "The private building status row should contain an icon item.");
        expect(overlay.rows[0].items[0].icon.source == StructureOverlayIconSource::UITexture,
            "The private building owner indicator should use a UI texture icon.");
        expect(overlay.rows[0].items[0].icon.textureName == "shield_black",
            "A black-owned private building should display the black shield icon.");
    }

    void testSelectedStructureOverlayPublicBuildingsUseOccupationIndicator() {
        GameConfig config;
        Board board;
        board.init(5);

        Building church = makeTestPublicBuilding(BuildingType::Church, {1, 1}, 2, 1);
        Kingdom white(KingdomId::White);
        white.addPiece(Piece(41, PieceType::Pawn, KingdomId::White, {1, 1}));
        Kingdom black(KingdomId::Black);
        black.addPiece(Piece(42, PieceType::Pawn, KingdomId::Black, {2, 1}));
        board.getCell(1, 1).piece = &white.pieces.back();
        board.getCell(2, 1).piece = &black.pieces.back();

        StructureOverlayContext overlayContext;
        overlayContext.isSelected = false;
        const StructureOverlayStack overlay = buildStructureOverlay(
            church, board, config, overlayContext, makeWorldStructureOverlayPolicy());

        expect(overlay.rows.size() == 1,
            "Selected public buildings with occupation should expose a single occupation row.");
        expect(overlay.rows[0].items.size() == 1,
            "The public occupation row should currently contain one occupation icon.");
        expect(overlay.rows[0].items[0].icon.textureName == "crossed_swords",
            "Contested public buildings should display the crossed swords occupation icon.");
    }

    void testSelectedStructureOverlayProducingBarracksAddsProgressRow() {
        GameConfig config;
        Board board;
        board.init(5);

        Building barracks = makeTestBarracks(8, KingdomId::White, {1, 1}, config);
        barracks.isProducing = true;
        barracks.producingType = static_cast<int>(PieceType::Knight);
        const int totalTurns = config.getProductionTurns(PieceType::Knight);
        barracks.turnsRemaining = totalTurns > 1 ? totalTurns - 1 : 0;

        StructureOverlayContext overlayContext;
        overlayContext.isSelected = false;
        const StructureOverlayStack overlay = buildStructureOverlay(
            barracks, board, config, overlayContext, makeWorldStructureOverlayPolicy());

        expect(overlay.rows.size() == 2,
            "Producing barracks should expose both the owner row and the production row.");
        expect(overlay.rows[1].placement == StructureOverlayRowPlacement::Below,
            "The production row should be stacked below the primary status row.");
        expect(overlay.rows[1].items.size() == 2,
            "The production row should contain a piece icon plus a progress bar.");
        expect(overlay.rows[1].items[0].type == StructureOverlayItemType::Icon,
            "The first production-row item should be the produced piece icon.");
        expect(overlay.rows[1].items[0].icon.source == StructureOverlayIconSource::PieceTexture,
            "Produced pieces should use piece textures in the overlay row.");
        expect(overlay.rows[1].items[0].icon.pieceType == PieceType::Knight,
            "The produced piece icon should match the barracks production type.");
        expect(overlay.rows[1].items[1].type == StructureOverlayItemType::ProgressBar,
            "The second production-row item should be the progress bar.");

        const float expectedProgress = totalTurns > 0
         ? static_cast<float>(totalTurns - barracks.turnsRemaining) / static_cast<float>(totalTurns)
         : 1.f;
        expect(std::abs(overlay.rows[1].items[1].progress - expectedProgress) < 0.0001f,
            "The production progress bar should expose completed progress derived from total turns and turns remaining.");
        expect(overlay.rows[1].items[1].text == formatTurnsRemainingLabel(barracks.turnsRemaining),
            "The production progress bar should carry the remaining-turns label.");
    }

    void testOverlayPolicyCanHideIndicatorsUntilSelected() {
        GameConfig config;
        Board board;
        board.init(5);

        Building barracks = makeTestBarracks(9, KingdomId::White, {1, 1}, config);
        barracks.isProducing = true;
        barracks.producingType = static_cast<int>(PieceType::Pawn);
        barracks.turnsRemaining = config.getProductionTurns(PieceType::Pawn);

        StructureOverlayPolicy policy;
        policy.occupationVisibility = StructureOverlayVisibility::WhenSelected;
        policy.barracksProductionVisibility = StructureOverlayVisibility::WhenSelected;

        StructureOverlayContext hiddenContext;
        hiddenContext.isSelected = false;
        expect(buildStructureOverlay(barracks, board, config, hiddenContext, policy).isEmpty(),
               "WhenSelected visibility should allow callers to hide all current indicators while the building is not selected.");

        StructureOverlayContext visibleContext;
        visibleContext.isSelected = true;
        const StructureOverlayStack visibleOverlay = buildStructureOverlay(
            barracks, board, config, visibleContext, policy);
        expect(visibleOverlay.rows.size() == 2,
               "WhenSelected visibility should restore both current indicators once the building becomes selected.");
    }

    void testOverlayPolicyAlwaysKeepsCurrentIndicatorsVisible() {
        GameConfig config;
        Board board;
        board.init(5);

        Building barracks = makeTestBarracks(10, KingdomId::White, {1, 1}, config);
        barracks.isProducing = true;
        barracks.producingType = static_cast<int>(PieceType::Bishop);
        barracks.turnsRemaining = config.getProductionTurns(PieceType::Bishop);

        StructureOverlayContext overlayContext;
        overlayContext.isSelected = false;
        const StructureOverlayStack overlay = buildStructureOverlay(
            barracks, board, config, overlayContext, makeWorldStructureOverlayPolicy());

        expect(overlay.rows.size() == 2,
               "The current world-overlay policy should keep both the ownership and production indicators visible even when the building is not selected.");
    }

void testSessionConfigDefaults() {
    GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsAI, "session_test");

    expect(controllerFor(session, KingdomId::White) == ControllerType::Human,
           "White kingdom should default to a human in Human vs AI preset.");
    expect(controllerFor(session, KingdomId::Black) == ControllerType::AI,
           "Black kingdom should default to an AI in Human vs AI preset.");
    expect(gameModeFromSession(session) == GameMode::HumanVsAI,
           "Derived session mode should match the preset.");
}

void testSessionValidatorRejectsInvalidOrdering() {
    GameSessionConfig invalid = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "invalid_session");
    invalid.kingdoms[0].kingdom = KingdomId::Black;

    std::string error;
    expect(!GameStateValidator::validateSessionConfig(invalid, &error),
           "Validator should reject invalid kingdom ordering.");
    expect(!error.empty(), "Validator should explain invalid session ordering.");
}

    void testSessionValidatorRejectsInvalidMultiplayerControllers() {
        GameSessionConfig invalid = makeDefaultGameSessionConfig(GameMode::HumanVsAI, "invalid_multiplayer_controllers");
        invalid.multiplayer.enabled = true;
        invalid.multiplayer.port = 45000;
        invalid.multiplayer.passwordHash = "hash";
        invalid.multiplayer.passwordSalt = "salt";

        std::string error;
        expect(!GameStateValidator::validateSessionConfig(invalid, &error),
            "Validator should reject multiplayer when both kingdoms are not human-controlled.");
        expect(!error.empty(), "Validator should explain invalid multiplayer controller setup.");
    }

    void testSessionValidatorRejectsInvalidMultiplayerPort() {
        GameSessionConfig invalid = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "invalid_multiplayer_port");
        invalid.multiplayer.enabled = true;
        invalid.multiplayer.port = 70000;
        invalid.multiplayer.passwordHash = "hash";
        invalid.multiplayer.passwordSalt = "salt";

        std::string error;
        expect(!GameStateValidator::validateSessionConfig(invalid, &error),
            "Validator should reject multiplayer ports outside the valid TCP range.");
        expect(!error.empty(), "Validator should explain invalid multiplayer port.");
    }

    void testLocalPlayerContextModes() {
        GameSessionConfig hotseat = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "hotseat_session");
        const LocalPlayerContext hotseatContext = makeLocalPlayerContextForSession(hotseat);
        expect(hotseatContext.mode == LocalSessionMode::LocalOnly,
            "Local Human vs Human sessions should stay in local-only mode.");
        expect(hotseatContext.isLocallyControlled(KingdomId::White)
         && hotseatContext.isLocallyControlled(KingdomId::Black),
            "Hotseat sessions should expose both kingdoms as local.");

        GameSessionConfig host = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "host_session");
        host.multiplayer.enabled = true;
        host.multiplayer.port = 42000;
        host.multiplayer.passwordHash = "hash";
        host.multiplayer.passwordSalt = "salt";
        const LocalPlayerContext hostContext = makeLocalPlayerContextForSession(host);
        expect(hostContext.mode == LocalSessionMode::LanHost,
            "Multiplayer sessions started from a save should default to host mode.");
        expect(hostContext.isLocallyControlled(KingdomId::White)
         && !hostContext.isLocallyControlled(KingdomId::Black),
            "LAN host sessions should only expose the White kingdom as local.");

        const LocalPlayerContext clientContext = makeLanClientLocalPlayerContext();
        expect(clientContext.mode == LocalSessionMode::LanClient,
            "Client helper should configure LAN client mode.");
        expect(!clientContext.isLocallyControlled(KingdomId::White)
         && clientContext.isLocallyControlled(KingdomId::Black),
            "LAN client sessions should only expose the Black kingdom as local.");
    }

void testGameEngineRestoresFactoryIds() {
    GameConfig config;
    GameEngine engine;
    GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "engine_restore_test");

    std::string error;
    expect(engine.startNewSession(session, config, &error), error);
    expect(engine.validate(&error), error);

    SaveData save = engine.createSaveData();
    save.gameName = "engine_restore_test";
    save.refreshLegacyMetadataFromSession();

    const sf::Vector2i emptyCell = findEmptyTraversableCell(engine);
    save.kingdoms[kingdomIndex(KingdomId::White)].pieces.push_back(
        Piece(42, PieceType::Pawn, KingdomId::White, emptyCell));

    GameEngine restored;
    expect(restored.restoreFromSave(save, config, &error), error);
    Piece nextPiece = restored.pieceFactory().createPiece(PieceType::Pawn, KingdomId::White, {0, 0});
    expect(nextPiece.id == 43, "Piece factory should continue after the highest restored piece ID.");
}

    void testGameEngineAssignsWorldSeed() {
        GameConfig config;
        GameEngine engine;
        GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsAI, "engine_world_seed_test");
        session.worldSeed = 0;

        std::string error;
        expect(engine.startNewSession(session, config, &error), error);
        expect(engine.sessionConfig().worldSeed != 0,
            "Starting a new session without a seed should generate a non-zero world seed.");

        const SaveData save = engine.createSaveData();
        expect(save.worldSeed == engine.sessionConfig().worldSeed,
            "Save data should persist the runtime world seed.");
    }

    void testBoardGeneratorUsesDeterministicSeed() {
        GameConfig config;

        Board boardA;
        boardA.init(25);
        std::vector<Building> buildingsA;
        const GenerationResult generationA = BoardGenerator::generate(boardA, config, buildingsA, 1337u);

        Board boardB;
        boardB.init(25);
        std::vector<Building> buildingsB;
        const GenerationResult generationB = BoardGenerator::generate(boardB, config, buildingsB, 1337u);

        Board boardC;
        boardC.init(25);
        std::vector<Building> buildingsC;
        const GenerationResult generationC = BoardGenerator::generate(boardC, config, buildingsC, 7331u);

        expect(buildGenerationSignature(boardA, buildingsA, generationA) == buildGenerationSignature(boardB, buildingsB, generationB),
            "The same world seed should reproduce identical terrain, public buildings, and spawn cells.");
        expect(buildGenerationSignature(boardA, buildingsA, generationA) != buildGenerationSignature(boardC, buildingsC, generationC),
            "Different world seeds should change the generated world.");
    }

    void testBoardGeneratorProducesGrassDominantTerrain() {
        GameConfig config;
        Board board;
        board.init(25);
        std::vector<Building> publicBuildings;
        const GenerationResult generation = BoardGenerator::generate(board, config, publicBuildings, 424242u);

        int traversableCells = 0;
        int grassCells = 0;
        int dirtCells = 0;
        int waterCells = 0;
        const int diameter = board.getDiameter();

        for (int y = 0; y < diameter; ++y) {
         for (int x = 0; x < diameter; ++x) {
             const Cell& cell = board.getCell(x, y);
             if (!cell.isInCircle) {
              continue;
             }

             ++traversableCells;
             if (cell.type == CellType::Grass) ++grassCells;
             if (cell.type == CellType::Dirt) ++dirtCells;
             if (cell.type == CellType::Water) ++waterCells;
         }
        }

        expect(grassCells > (dirtCells + waterCells),
            "Generated terrain should remain grass-dominant overall.");
        expect(dirtCells > 0 && dirtCells < (traversableCells / 3),
            "Generated terrain should contain a few dirt patches without overwhelming the map.");
        expect(waterCells > 0 && waterCells < (traversableCells / 8),
            "Generated terrain should contain only a small amount of water.");
        expect(largestConnectedRegion(board, CellType::Water) <= 40,
            "Water should remain limited to small lakes rather than giant connected regions.");
        expect(board.getCell(generation.playerSpawn.x, generation.playerSpawn.y).type != CellType::Water,
            "Player spawn must never be placed on water.");
        expect(board.getCell(generation.aiSpawn.x, generation.aiSpawn.y).type != CellType::Water,
            "AI spawn must never be placed on water.");
        expect(board.getCell(generation.playerSpawn.x, generation.playerSpawn.y).building == nullptr,
            "Player spawn must never overlap a public building.");
        expect(board.getCell(generation.aiSpawn.x, generation.aiSpawn.y).building == nullptr,
            "AI spawn must never overlap a public building.");
        expectPublicBuildingsAvoidWater(board, publicBuildings);
    }

void testSaveManagerRoundTrip() {
    SaveData data;
    data.gameName = "save_roundtrip";
    data.turnNumber = 7;
    data.activeKingdom = KingdomId::Black;
    data.mapRadius = 5;
        data.worldSeed = 123456789u;
    data.sessionKingdoms = defaultKingdomParticipants(GameMode::HumanVsHuman);
    data.sessionKingdoms[0].participantName = "Player \"Alpha\"";
    data.sessionKingdoms[1].participantName = "Player Beta";
    data.multiplayer.enabled = true;
    data.multiplayer.port = 42000;
    data.multiplayer.passwordHash = "argon2id$example_hash";
    data.multiplayer.passwordSalt = "example_salt";
    data.refreshLegacyMetadataFromSession();

    data.grid = {
        {
            {CellType::Grass, true, 1},
            {CellType::Water, true, 2}
        },
        {
            {CellType::Void, false, 0},
            {CellType::Dirt, true, 3}
        }
    };

    data.kingdoms[0].id = KingdomId::White;
    data.kingdoms[0].gold = 120;
    data.kingdoms[0].pieces.push_back(Piece(0, PieceType::King, KingdomId::White, {0, 0}));
    Building ownedBarracks = makeTestBarracks(10, KingdomId::White, {1, 1}, GameConfig{});
    ownedBarracks.rotationQuarterTurns = 1;
    ownedBarracks.flipMask = 0;
    data.kingdoms[0].buildings.push_back(ownedBarracks);
    data.kingdoms[1].id = KingdomId::Black;
    data.kingdoms[1].gold = 95;
    data.kingdoms[1].pieces.push_back(Piece(1, PieceType::King, KingdomId::Black, {1, 0}));
    Building publicMine;
    publicMine.id = 20;
    publicMine.type = BuildingType::Mine;
    publicMine.owner = KingdomId::White;
    publicMine.isNeutral = true;
    publicMine.origin = {3, 4};
    publicMine.width = 6;
    publicMine.height = 4;
    publicMine.rotationQuarterTurns = 3;
    publicMine.flipMask = 2;
    publicMine.cellHP.assign(publicMine.width * publicMine.height, 1);
    data.publicBuildings.push_back(publicMine);
    data.events.push_back({7, KingdomId::Black, "AI said \"check\" and held {center}."});

    SaveManager manager;
    const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "anormalchess_save_test.json";
    expect(manager.save(tempPath.string(), data), "SaveManager should write a save file.");

    SaveData loaded;
    expect(manager.load(tempPath.string(), loaded), "SaveManager should reload the written save.");
    std::filesystem::remove(tempPath);

    expect(loaded.grid.size() == 2 && loaded.grid[0].size() == 2,
           "Grid data should round-trip through SaveManager.");
        expect(loaded.grid[0][0].terrainFlipMask == data.grid[0][0].terrainFlipMask
            && loaded.grid[1][1].terrainFlipMask == data.grid[1][1].terrainFlipMask,
            "Terrain flip masks should round-trip through SaveManager.");
    expect(loaded.events.size() == 1 && loaded.events[0].message == data.events[0].message,
           "Event messages should preserve escaped characters.");
    expect(loaded.sessionKingdoms[0].participantName == data.sessionKingdoms[0].participantName,
           "Session participant names should round-trip through SaveManager.");
        expect(loaded.worldSeed == data.worldSeed,
            "World seed should round-trip through SaveManager.");
        expect(loaded.controllers[0] == ControllerType::Human && loaded.controllers[1] == ControllerType::Human,
           "Legacy controller metadata should stay aligned with session metadata.");
        expect(loaded.multiplayer.enabled && loaded.multiplayer.port == data.multiplayer.port,
            "Multiplayer metadata should round-trip through SaveManager.");
        expect(loaded.multiplayer.passwordHash == data.multiplayer.passwordHash,
            "Multiplayer password hash should round-trip through SaveManager.");
        expect(loaded.kingdoms[0].buildings.size() == 1
            && loaded.kingdoms[0].buildings[0].rotationQuarterTurns == ownedBarracks.rotationQuarterTurns,
            "Owned building rotations should round-trip through SaveManager.");
        expect(loaded.publicBuildings.size() == 1
            && loaded.publicBuildings[0].flipMask == publicMine.flipMask
            && loaded.publicBuildings[0].rotationQuarterTurns == publicMine.rotationQuarterTurns,
            "Public building transforms should round-trip through SaveManager.");
}

        void testSaveManagerStringRoundTrip() {
            SaveData data;
            data.gameName = "save_string_roundtrip";
            data.turnNumber = 3;
            data.activeKingdom = KingdomId::White;
            data.mapRadius = 4;
            data.worldSeed = 987654321u;
            data.sessionKingdoms = defaultKingdomParticipants(GameMode::HumanVsHuman);
            data.multiplayer.enabled = true;
            data.multiplayer.port = 41000;
            data.multiplayer.passwordSalt = "salt";
            data.multiplayer.passwordHash = MultiplayerPasswordUtils::computePasswordDigest("secret", data.multiplayer.passwordSalt);
            data.refreshLegacyMetadataFromSession();

            SaveManager manager;
            const std::string serialized = manager.serialize(data);
            expect(!serialized.empty(), "SaveManager should serialize save data to a non-empty string.");

            SaveData loaded;
            expect(manager.deserialize(serialized, loaded), "SaveManager should deserialize save data from a string snapshot.");
            expect(loaded.gameName == data.gameName, "Serialized string snapshots should preserve the game name.");
            expect(loaded.worldSeed == data.worldSeed, "Serialized string snapshots should preserve the world seed.");
            expect(loaded.multiplayer.port == data.multiplayer.port, "Serialized string snapshots should preserve multiplayer metadata.");
        }

        void testMultiplayerPasswordDigest() {
            const std::string salt = MultiplayerPasswordUtils::generateSalt();
            expect(!salt.empty(), "Generated multiplayer salts should not be empty.");

            const std::string digestA = MultiplayerPasswordUtils::computePasswordDigest("secret", salt);
            const std::string digestB = MultiplayerPasswordUtils::computePasswordDigest("secret", salt);
            const std::string digestC = MultiplayerPasswordUtils::computePasswordDigest("different", salt);

            expect(digestA == digestB, "Password digests should be deterministic for the same password and salt.");
            expect(digestA != digestC, "Password digests should change when the password changes.");
        }

        void testMultiplayerTurnSubmissionPacketRoundTrip() {
            TurnCommand command;
            command.type = TurnCommand::Build;
            command.pieceId = 42;
            command.origin = {1, 2};
            command.destination = {3, 4};
            command.buildingType = BuildingType::Barracks;
            command.buildOrigin = {5, 6};
            command.buildRotationQuarterTurns = 3;
            command.barracksId = 7;
            command.produceType = PieceType::Knight;
            command.upgradePieceId = 8;
            command.upgradeTarget = PieceType::Rook;
            command.formationId = 9;

            sf::Packet packet = createPacket(MultiplayerMessageType::TurnSubmission);
            expect(writePacket(packet, MultiplayerTurnSubmission{{command}}),
                "Protocol should serialize turn submission packets.");

            MultiplayerMessageType type = MultiplayerMessageType::ServerInfoRequest;
            expect(extractMessageType(packet, type), "Protocol should decode packet types.");
            expect(type == MultiplayerMessageType::TurnSubmission,
                "Packet type should remain the submitted multiplayer message type.");

            MultiplayerTurnSubmission submission;
            expect(readPacket(packet, submission), "Protocol should deserialize turn submission packets.");
            expect(submission.commands.size() == 1, "Turn submission packets should preserve command counts.");
            expect(submission.commands[0].buildOrigin == command.buildOrigin,
                "Turn submission packets should preserve command payloads.");
            expect(submission.commands[0].buildRotationQuarterTurns == command.buildRotationQuarterTurns,
                "Turn submission packets should preserve build rotation payloads.");
            expect(submission.commands[0].upgradeTarget == command.upgradeTarget,
                "Turn submission packets should preserve enum payloads.");
        }

        void testMultiplayerTurnRejectedPacketRoundTrip() {
            sf::Packet packet = createPacket(MultiplayerMessageType::TurnRejected);
            expect(writePacket(packet, MultiplayerTurnRejected{"Rejected test turn."}),
                "Protocol should serialize turn rejection packets.");

            MultiplayerMessageType type = MultiplayerMessageType::ServerInfoRequest;
            expect(extractMessageType(packet, type), "Protocol should decode turn rejection packet types.");
            expect(type == MultiplayerMessageType::TurnRejected,
                "Turn rejection packets should preserve their multiplayer message type.");

            MultiplayerTurnRejected rejection;
            expect(readPacket(packet, rejection), "Protocol should deserialize turn rejection packets.");
            expect(rejection.reason == "Rejected test turn.",
                "Turn rejection packets should preserve the rejection reason.");
        }

        void testMultiplayerLoopbackSmoke() {
            MultiplayerConfig config;
            config.enabled = true;
            config.passwordSalt = "loopback_salt";
            config.passwordHash = MultiplayerPasswordUtils::computePasswordDigest("secret", config.passwordSalt);

            MultiplayerServer server;
            unsigned short port = 47000;
            bool started = false;
            for (; port < 47100; ++port) {
                std::string startError;
                if (server.start(port, "loopback_save", config, &startError)) {
                    started = true;
                    break;
                }
            }
            expect(started, "Loopback multiplayer test could not find a free local port.");

            MultiplayerClient client;
            std::string error;
            expect(client.connect(sf::IpAddress::LocalHost, port, sf::seconds(1.f), &error),
                   "Loopback multiplayer client should connect to the local server.");
            expect(client.requestServerInfo(&error),
                   "Loopback multiplayer client should be able to request server info.");

            MultiplayerServerInfo info;
            bool receivedInfo = false;
            expect(waitUntil([&]() {
                server.update();
                client.update();

                while (client.hasPendingEvent()) {
                    const auto event = client.popNextEvent();
                    if (event.type == MultiplayerClient::Event::Type::ServerInfoReceived) {
                        info = event.serverInfo;
                        receivedInfo = true;
                        return true;
                    }
                }

                return false;
            }), "Loopback multiplayer test should receive server info.");
            expect(receivedInfo && info.joinable,
                   "Loopback multiplayer server info should report a joinable session.");

            expect(client.sendJoinRequest(MultiplayerPasswordUtils::computePasswordDigest("secret", info.passwordSalt), &error),
                   "Loopback multiplayer client should be able to send a join request.");

            bool joinAccepted = false;
            bool serverConnectedEvent = false;
            expect(waitUntil([&]() {
                server.update();
                client.update();

                while (server.hasPendingEvent()) {
                    const auto event = server.popNextEvent();
                    if (event.type == MultiplayerServer::Event::Type::ClientConnected) {
                        serverConnectedEvent = true;
                    }
                }

                while (client.hasPendingEvent()) {
                    const auto event = client.popNextEvent();
                    if (event.type == MultiplayerClient::Event::Type::JoinAccepted) {
                        joinAccepted = true;
                    }
                }

                return joinAccepted && serverConnectedEvent;
            }), "Loopback multiplayer join handshake should complete.");

            expect(server.sendSnapshot("{\"snapshot\":true}", &error),
                   "Loopback multiplayer server should send snapshots after join.");

            bool receivedSnapshot = false;
            expect(waitUntil([&]() {
                client.update();
                while (client.hasPendingEvent()) {
                    const auto event = client.popNextEvent();
                    if (event.type == MultiplayerClient::Event::Type::SnapshotReceived) {
                        receivedSnapshot = (event.serializedSaveData == "{\"snapshot\":true}");
                        return receivedSnapshot;
                    }
                }

                return false;
            }), "Loopback multiplayer client should receive state snapshots.");

            TurnCommand command;
            command.type = TurnCommand::Move;
            command.pieceId = 17;
            command.origin = {1, 1};
            command.destination = {2, 2};
            expect(client.sendTurnSubmission({command}, &error),
                   "Loopback multiplayer client should send turn submissions.");

            bool receivedTurn = false;
            expect(waitUntil([&]() {
                server.update();
                while (server.hasPendingEvent()) {
                    const auto event = server.popNextEvent();
                    if (event.type == MultiplayerServer::Event::Type::TurnSubmitted) {
                        receivedTurn = event.commands.size() == 1
                            && event.commands[0].pieceId == command.pieceId
                            && event.commands[0].destination == command.destination;
                        return receivedTurn;
                    }
                }

                return false;
            }), "Loopback multiplayer server should receive remote turn submissions.");

            client.disconnect();
            server.stop();
        }

        void testMultiplayerServerTreatsPreAuthDisconnectAsInterruptedConnection() {
            MultiplayerConfig config;
            config.enabled = true;
            config.passwordSalt = "preauth_salt";
            config.passwordHash = MultiplayerPasswordUtils::computePasswordDigest("secret", config.passwordSalt);

            MultiplayerServer server;
            const unsigned short port = startLoopbackServerOnFreePort(
                server,
                config,
                "preauth_interrupt_save",
                47100,
                47200,
                "Pre-auth reconnect test could not find a free local port.");

            MultiplayerClient client;
            std::string error;
            expect(client.connect(sf::IpAddress::LocalHost, port, sf::seconds(1.f), &error),
                   "Pre-auth reconnect test client should connect to the local server.");
            expect(client.requestServerInfo(&error),
                   "Pre-auth reconnect test client should request server info.");

            bool receivedInfo = false;
            expect(waitUntil([&]() {
                server.update();
                client.update();

                while (client.hasPendingEvent()) {
                    const auto event = client.popNextEvent();
                    if (event.type == MultiplayerClient::Event::Type::ServerInfoReceived) {
                        receivedInfo = true;
                        return true;
                    }
                }

                return false;
            }), "Pre-auth reconnect test should receive server info before disconnecting.");
            expect(receivedInfo, "Pre-auth reconnect test should complete the initial ping stage.");

            client.disconnect();

            MultiplayerServer::Event serverEvent;
            expect(waitUntil([&]() {
                server.update();
                while (server.hasPendingEvent()) {
                    const auto event = server.popNextEvent();
                    if (event.type == MultiplayerServer::Event::Type::ClientConnectionInterrupted
                        || event.type == MultiplayerServer::Event::Type::ClientDisconnected) {
                        serverEvent = event;
                        return true;
                    }
                }

                return false;
            }), "Pre-auth reconnect test server should observe the interrupted connection.");

            expect(serverEvent.type == MultiplayerServer::Event::Type::ClientConnectionInterrupted,
                   "Disconnects before join authentication must not be promoted to gameplay-level client disconnections.");

            server.stop();
        }

        void testMultiplayerServerReportsAuthenticatedDisconnect() {
            MultiplayerConfig config;
            config.enabled = true;
            config.passwordSalt = "authenticated_disconnect_salt";
            config.passwordHash = MultiplayerPasswordUtils::computePasswordDigest("secret", config.passwordSalt);

            MultiplayerServer server;
            const unsigned short port = startLoopbackServerOnFreePort(
                server,
                config,
                "authenticated_disconnect_save",
                47200,
                47300,
                "Authenticated disconnect test could not find a free local port.");

            MultiplayerClient client;
            connectAndAuthenticateLoopbackClient(server, client, port, "secret", "Authenticated disconnect test");
            client.disconnect();

            MultiplayerServer::Event serverEvent;
            expect(waitUntil([&]() {
                server.update();
                while (server.hasPendingEvent()) {
                    const auto event = server.popNextEvent();
                    if (event.type == MultiplayerServer::Event::Type::ClientConnectionInterrupted
                        || event.type == MultiplayerServer::Event::Type::ClientDisconnected) {
                        serverEvent = event;
                        return true;
                    }
                }

                return false;
            }), "Authenticated disconnect test server should observe the remote disconnect.");

            expect(serverEvent.type == MultiplayerServer::Event::Type::ClientDisconnected,
                   "Authenticated players disconnecting mid-session must still produce a gameplay disconnect event.");

            server.stop();
        }

        void testMultiplayerReconnectReusesSameClientInstance() {
            MultiplayerConfig config;
            config.enabled = true;
            config.passwordSalt = "reconnect_salt";
            config.passwordHash = MultiplayerPasswordUtils::computePasswordDigest("secret", config.passwordSalt);

            MultiplayerServer server;
            const unsigned short port = startLoopbackServerOnFreePort(
                server,
                config,
                "reconnect_save",
                47300,
                47400,
                "Reconnect smoke test could not find a free local port.");

            MultiplayerClient client;
            connectAndAuthenticateLoopbackClient(server, client, port, "secret", "Reconnect smoke test initial join");

            client.disconnect();
            expect(waitUntil([&]() {
                server.update();
                while (server.hasPendingEvent()) {
                    const auto event = server.popNextEvent();
                    if (event.type == MultiplayerServer::Event::Type::ClientDisconnected) {
                        return true;
                    }
                }

                return false;
            }), "Reconnect smoke test should observe the first authenticated disconnect before retrying.");

            connectAndAuthenticateLoopbackClient(server, client, port, "secret", "Reconnect smoke test reused client");

            std::string error;
            expect(server.sendSnapshot("{\"reconnected\":true}", &error),
                   "Reconnect smoke test server should send snapshots to the reconnected client.");

            bool receivedSnapshot = false;
            expect(waitUntil([&]() {
                client.update();
                while (client.hasPendingEvent()) {
                    const auto event = client.popNextEvent();
                    if (event.type == MultiplayerClient::Event::Type::SnapshotReceived) {
                        receivedSnapshot = (event.serializedSaveData == "{\"reconnected\":true}");
                        return receivedSnapshot;
                    }
                }

                return false;
            }), "Reconnect smoke test should receive a snapshot after reconnecting with the same client instance.");

            client.disconnect();
            server.stop();
        }

void testTurnSystemSkipsUnaffordableBuild() {
    GameConfig config;
    Board board;
    board.init(5);

    Kingdom white(KingdomId::White);
    Kingdom black(KingdomId::Black);
    std::vector<Building> publicBuildings;
    TurnSystem turnSystem;
    turnSystem.setActiveKingdom(KingdomId::White);

    TurnCommand buildCommand;
    buildCommand.type = TurnCommand::Build;
    buildCommand.buildingType = BuildingType::Barracks;
    buildCommand.buildOrigin = {2, 2};
    expect(turnSystem.queueCommand(buildCommand), "Build command should be queueable during planning.");

    EventLog eventLog;
    PieceFactory pieceFactory;
    BuildingFactory buildingFactory;
    turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

    expect(white.gold == 0, "Unaffordable commands must not change gold.");
    expect(white.buildings.empty(), "Unaffordable build commands must not create buildings.");
}

void testTurnSystemSkipsUnaffordableProduction() {
    GameConfig config;
    Board board;
    board.init(5);

    Kingdom white(KingdomId::White);
    Kingdom black(KingdomId::Black);
    white.addBuilding(makeTestBarracks(7, KingdomId::White, {0, 0}, config));

    std::vector<Building> publicBuildings;
    TurnSystem turnSystem;
    turnSystem.setActiveKingdom(KingdomId::White);

    TurnCommand produceCommand;
    produceCommand.type = TurnCommand::Produce;
    produceCommand.barracksId = 7;
    produceCommand.produceType = PieceType::Pawn;
    expect(turnSystem.queueCommand(produceCommand), "Produce command should be queueable during planning.");

    EventLog eventLog;
    PieceFactory pieceFactory;
    BuildingFactory buildingFactory;
    turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

    expect(white.gold == 0, "Unaffordable production must not change gold.");
    expect(!white.buildings.front().isProducing, "Unaffordable production must not start barracks production.");
}

void testTurnSystemSkipsUnaffordableUpgrade() {
    GameConfig config;
    Board board;
    board.init(5);

    Kingdom white(KingdomId::White);
    Kingdom black(KingdomId::Black);
    Piece pawn(11, PieceType::Pawn, KingdomId::White, {2, 2});
    pawn.xp = config.getXPThresholdPawnToKnightOrBishop();
    white.addPiece(pawn);
    board.getCell(2, 2).piece = &white.pieces.back();

    std::vector<Building> publicBuildings;
    TurnSystem turnSystem;
    turnSystem.setActiveKingdom(KingdomId::White);

    TurnCommand upgradeCommand;
    upgradeCommand.type = TurnCommand::Upgrade;
    upgradeCommand.upgradePieceId = 11;
    upgradeCommand.upgradeTarget = PieceType::Knight;
    expect(turnSystem.queueCommand(upgradeCommand), "Upgrade command should be queueable during planning.");

    EventLog eventLog;
    PieceFactory pieceFactory;
    BuildingFactory buildingFactory;
    turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

    expect(white.gold == 0, "Unaffordable upgrades must not change gold.");
    expect(white.pieces.front().type == PieceType::Pawn,
           "Unaffordable upgrades must not change the piece type.");
}

void testAIStrategySpecialDoesNotMutateRuntimeGold() {
    GameConfig config;
    AIConfig aiConfig;
    Board board;
    board.init(5);

    Kingdom white(KingdomId::White);
    white.addPiece(Piece(1, PieceType::King, KingdomId::White, {2, 2}));
    Piece pawn(2, PieceType::Pawn, KingdomId::White, {1, 2});
    pawn.xp = config.getXPThresholdPawnToKnightOrBishop();
    white.addPiece(pawn);
    white.gold = config.getUpgradeCost(PieceType::Pawn, PieceType::Knight) + 5;

    Kingdom black(KingdomId::Black);
    black.addPiece(Piece(3, PieceType::King, KingdomId::Black, {4, 4}));

    const int initialGold = white.gold;
    const AIBrain brain;
    const auto commands = AIStrategySpecial::decide(
        board,
        white,
        black,
        {},
        config,
        aiConfig,
        brain,
        false);

    expect(!commands.empty(), "Special AI should still plan an upgrade when one is affordable.");
    expect(commands.front().type == TurnCommand::Upgrade,
           "Special AI should emit an upgrade command for an eligible pawn.");
    expect(white.gold == initialGold,
           "Special AI planning must not mutate the runtime kingdom gold reserve.");
}

void testGameStateValidatorRejectsNegativeSaveGold() {
    GameConfig config;
    GameEngine engine;
    GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "negative_save_gold_test");

    std::string error;
    expect(engine.startNewSession(session, config, &error), error);

    SaveData data = engine.createSaveData();
    data.kingdoms[kingdomIndex(KingdomId::White)].gold = -1;

    expect(!GameStateValidator::validateSaveData(data, &error),
           "Save validation should reject negative kingdom gold.");
    expect(!error.empty(), "Save validation should explain negative gold failures.");

    GameEngine restored;
    expect(!restored.restoreFromSave(data, config, &error),
           "Restoring a save with negative gold should fail validation.");
}

void testGameStateValidatorRejectsNegativeRuntimeGold() {
    GameConfig config;
    GameEngine engine;
    GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "negative_runtime_gold_test");

    std::string error;
    expect(engine.startNewSession(session, config, &error), error);
    engine.kingdom(KingdomId::White).gold = -1;

    expect(!engine.validate(&error), "Runtime validation should reject negative kingdom gold.");
    expect(!error.empty(), "Runtime validation should explain negative gold failures.");
}

void testGameConfigClampsNegativeEconomyValues() {
    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "anormalchess_negative_economy_test.json";
    {
        std::ofstream out(tempPath);
        out << "{\n"
            << "  \"economy\": {\n"
            << "    \"starting_gold\": -10,\n"
            << "    \"mine_income_per_cell_per_turn\": -3,\n"
            << "    \"farm_income_per_cell_per_turn\": -2,\n"
            << "    \"barracks_cost\": -50,\n"
            << "    \"wood_wall_cost\": -20,\n"
            << "    \"stone_wall_cost\": -40,\n"
            << "    \"arena_cost\": -60,\n"
            << "    \"pawn_recruit_cost\": -10,\n"
            << "    \"knight_recruit_cost\": -30,\n"
            << "    \"bishop_recruit_cost\": -30,\n"
            << "    \"rook_recruit_cost\": -60,\n"
            << "    \"upgrade_pawn_to_knight_cost\": -20,\n"
            << "    \"upgrade_pawn_to_bishop_cost\": -20,\n"
            << "    \"upgrade_to_rook_cost\": -50\n"
            << "  }\n"
            << "}\n";
    }

    GameConfig config;
    expect(config.loadFromFile(tempPath.string()), "GameConfig should load a negative-economy override file.");
    std::filesystem::remove(tempPath);

    expect(config.getStartingGold() == 0, "Negative starting gold should be clamped to zero.");
    expect(config.getMineIncomePerCellPerTurn() == 0, "Negative mine income should be clamped to zero.");
    expect(config.getFarmIncomePerCellPerTurn() == 0, "Negative farm income should be clamped to zero.");
    expect(config.getBarracksCost() == 0, "Negative barracks cost should be clamped to zero.");
    expect(config.getWoodWallCost() == 0, "Negative wood wall cost should be clamped to zero.");
    expect(config.getStoneWallCost() == 0, "Negative stone wall cost should be clamped to zero.");
    expect(config.getArenaCost() == 0, "Negative arena cost should be clamped to zero.");
    expect(config.getRecruitCost(PieceType::Pawn) == 0, "Negative recruit costs should be clamped to zero.");
    expect(config.getRecruitCost(PieceType::Knight) == 0, "Negative recruit costs should be clamped to zero.");
    expect(config.getRecruitCost(PieceType::Bishop) == 0, "Negative recruit costs should be clamped to zero.");
    expect(config.getRecruitCost(PieceType::Rook) == 0, "Negative recruit costs should be clamped to zero.");
    expect(config.getUpgradeCost(PieceType::Pawn, PieceType::Knight) == 0,
           "Negative upgrade costs should be clamped to zero.");
    expect(config.getUpgradeCost(PieceType::Pawn, PieceType::Bishop) == 0,
           "Negative upgrade costs should be clamped to zero.");
    expect(config.getUpgradeCost(PieceType::Knight, PieceType::Rook) == 0,
           "Negative upgrade costs should be clamped to zero.");
}

void testProjectedIncomeHelper() {
    GameConfig config;
    Board board;
    board.init(3);

    Kingdom white(KingdomId::White);
    white.addPiece(Piece(0, PieceType::Pawn, KingdomId::White, {1, 1}));

    Building mine;
    mine.type = BuildingType::Mine;
    mine.isNeutral = true;
    mine.origin = {1, 1};
    mine.width = 1;
    mine.height = 1;
    mine.cellHP = {1};

    std::vector<Building> publicBuildings = {mine};
    board.getCell(1, 1).piece = &white.pieces.back();

    const int projectedIncome = EconomySystem::calculateProjectedIncome(white, board, publicBuildings, config);
    expect(projectedIncome == config.getMineIncomePerCellPerTurn(),
           "Projected income should match occupied public resource cells.");

    Kingdom black(KingdomId::Black);
    black.addPiece(Piece(1, PieceType::Pawn, KingdomId::Black, {1, 1}));
    board.getCell(1, 1).piece = &black.pieces.back();
    const int contestedIncome = EconomySystem::calculateProjectedIncome(white, board, publicBuildings, config);
    expect(contestedIncome == 0,
           "Contested public resource cells should not produce projected income.");
}

    void testStructureChunkRegistry() {
        const StructureChunkDefinition* church = StructureChunkRegistry::find(BuildingType::Church);
        expect(church != nullptr, "Church should have a chunked structure definition.");
        expect(church->width == 4 && church->height == 3,
            "Church chunk definition should expose the expected 4x3 footprint.");
        expect(StructureChunkRegistry::makeChunkTextureRelativePath(BuildingType::Farm, 5, 3)
             == "/textures/cells/structures/farm/farm_6_4.png",
            "Farm chunk path generation should match the runtime asset layout.");
        expect(StructureChunkRegistry::makeChunkTextureKey(BuildingType::Mine, 5, 5)
             == "building_chunk_mine_6_6",
            "Mine chunk keys should be generated from local coordinates.");
        expect(!StructureChunkRegistry::hasChunkedTextures(BuildingType::Barracks),
            "Non-structure buildings should keep the legacy single-texture path.");
    }

    void testGameConfigAlignsChunkedStructureDimensions() {
        const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "anormalchess_structure_size_test.json";
        {
         std::ofstream out(tempPath);
         out << "{\n"
             << "  \"buildings\": {\n"
             << "    \"church_width\": 4,\n"
             << "    \"church_height\": 3,\n"
             << "    \"mine_width\": 6,\n"
             << "    \"mine_height\": 6,\n"
             << "    \"farm_width\": 4,\n"
             << "    \"farm_height\": 3\n"
             << "  }\n"
             << "}\n";
        }

        GameConfig config;
        expect(config.loadFromFile(tempPath.string()), "GameConfig should load a partial JSON override file.");
        std::filesystem::remove(tempPath);

        expect(config.getBuildingWidth(BuildingType::Farm) == 6,
            "Chunked farm definitions should force the runtime footprint width to 6.");
        expect(config.getBuildingHeight(BuildingType::Farm) == 4,
            "Chunked farm definitions should force the runtime footprint height to 4.");
    }

void testInGameViewModelBuilder() {
    GameConfig config;
    GameEngine engine;
    GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "view_model_test");

    std::string error;
    expect(engine.startNewSession(session, config, &error), error);
    engine.eventLog().log(1, KingdomId::White, "Opened with a pawn move.");

    const InGameViewModel model = buildInGameViewModel(engine, config, GameState::Playing, true);
    expect(model.turnNumber == 1, "Dashboard model should expose the active turn number.");
    expect(model.balanceMetrics[0].label == "Gold", "Dashboard model should expose kingdom balance labels.");
        expect(model.eventRows.size() >= 2, "Dashboard model should expose event history rows.");
        expect(model.eventRows.back().actionLabel == "Opened with a pawn move.",
            "Dashboard model should preserve chronological event text.");
        expect(model.eventRows.back().actorLabel.find("Player 1") != std::string::npos,
           "Event rows should use participant names in actor labels.");
    expect(!model.activeTurnLabel.empty(), "Dashboard model should expose the active turn label.");
}

}

int main() {
    const std::vector<std::pair<std::string, void(*)()>> tests = {
        {"session defaults", testSessionConfigDefaults},
        {"session validator", testSessionValidatorRejectsInvalidOrdering},
        {"multiplayer validator controllers", testSessionValidatorRejectsInvalidMultiplayerControllers},
        {"multiplayer validator port", testSessionValidatorRejectsInvalidMultiplayerPort},
        {"local player context", testLocalPlayerContextModes},
        {"engine restore factory sync", testGameEngineRestoresFactoryIds},
        {"engine world seed", testGameEngineAssignsWorldSeed},
        {"board generator deterministic seed", testBoardGeneratorUsesDeterministicSeed},
        {"board generator terrain balance", testBoardGeneratorProducesGrassDominantTerrain},
        {"layered selection priority", testLayeredSelectionStackResolvesPriority},
        {"layered selection building cycle", testLayeredSelectionStackSupportsBuildingTerrainCycle},
        {"layered selection preview override", testLayeredSelectionStackSupportsPreviewPieceOverride},
        {"public building occupation state", testPublicBuildingOccupationStateResolvesAllOutcomes},
        {"private building overlay owner shield", testSelectedStructureOverlayPrivateBuildingsUseOwnerShield},
        {"public building overlay occupation", testSelectedStructureOverlayPublicBuildingsUseOccupationIndicator},
        {"barracks overlay production row", testSelectedStructureOverlayProducingBarracksAddsProgressRow},
        {"overlay policy when selected visibility", testOverlayPolicyCanHideIndicatorsUntilSelected},
        {"overlay policy always visible", testOverlayPolicyAlwaysKeepsCurrentIndicatorsVisible},
        {"save manager roundtrip", testSaveManagerRoundTrip},
        {"save manager string roundtrip", testSaveManagerStringRoundTrip},
        {"multiplayer password digest", testMultiplayerPasswordDigest},
        {"multiplayer turn packet roundtrip", testMultiplayerTurnSubmissionPacketRoundTrip},
        {"multiplayer turn rejection packet roundtrip", testMultiplayerTurnRejectedPacketRoundTrip},
        {"multiplayer loopback smoke", testMultiplayerLoopbackSmoke},
        {"multiplayer pre-auth disconnect", testMultiplayerServerTreatsPreAuthDisconnectAsInterruptedConnection},
        {"multiplayer authenticated disconnect", testMultiplayerServerReportsAuthenticatedDisconnect},
        {"multiplayer reconnect same client", testMultiplayerReconnectReusesSameClientInstance},
        {"turn system affordability", testTurnSystemSkipsUnaffordableBuild},
        {"turn system unaffordable production", testTurnSystemSkipsUnaffordableProduction},
        {"turn system unaffordable upgrade", testTurnSystemSkipsUnaffordableUpgrade},
        {"ai special planning preserves gold", testAIStrategySpecialDoesNotMutateRuntimeGold},
        {"save validator negative gold", testGameStateValidatorRejectsNegativeSaveGold},
        {"runtime validator negative gold", testGameStateValidatorRejectsNegativeRuntimeGold},
        {"game config clamps negative economy", testGameConfigClampsNegativeEconomyValues},
        {"projected income helper", testProjectedIncomeHelper},
        {"structure chunk registry", testStructureChunkRegistry},
        {"chunked structure dimensions", testGameConfigAlignsChunkedStructureDimensions},
        {"in-game view model builder", testInGameViewModelBuilder},
    };

    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& ex) {
            std::cerr << "[FAIL] " << name << ": " << ex.what() << '\n';
            return 1;
        }
    }

    return 0;
}