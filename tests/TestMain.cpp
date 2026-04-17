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

#include "Autonomous/AutonomousUnit.hpp"
#include "AI/AIBrain.hpp"
#include "AI/ForwardModel.hpp"
#include "AI/AIStrategySpecial.hpp"
#include "Board/Board.hpp"
#include "Board/BoardGenerator.hpp"
#include "Board/CellTraversal.hpp"
#include "Board/CellType.hpp"
#include "Buildings/BuildingFactory.hpp"
#include "Buildings/StructurePlacementProfile.hpp"
#include "Buildings/StructureChunkRegistry.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/AIConfig.hpp"
#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Core/InteractionPermissions.hpp"
#include "Core/LocalPlayerContext.hpp"
#include "Core/GameState.hpp"
#include "Core/GameStateValidator.hpp"
#include "Core/TurnDraft.hpp"
#include "Debug/GameStateDebugRecorder.hpp"
#include "Input/InputHandler.hpp"
#include "Input/LayeredSelection.hpp"
#include "Render/Camera.hpp"
#include "Render/StructureOverlay.hpp"
#include "Multiplayer/MultiplayerClient.hpp"
#include "Multiplayer/MultiplayerRuntime.hpp"
#include "Multiplayer/PasswordUtils.hpp"
#include "Multiplayer/Protocol.hpp"
#include "Multiplayer/MultiplayerServer.hpp"
#include "Runtime/AITurnCoordinator.hpp"
#include "Runtime/BuildOverlayCoordinator.hpp"
#include "Runtime/FrontendCoordinator.hpp"
#include "Runtime/InGamePresentationCoordinator.hpp"
#include "Runtime/InputCoordinator.hpp"
#include "Runtime/InteractivePermissionsCache.hpp"
#include "Runtime/MultiplayerJoinCoordinator.hpp"
#include "Runtime/MultiplayerEventCoordinator.hpp"
#include "Runtime/MultiplayerRuntimeCoordinator.hpp"
#include "Runtime/PendingTurnValidationCache.hpp"
#include "Runtime/PanelActionCoordinator.hpp"
#include "Runtime/RenderCoordinator.hpp"
#include "Runtime/SelectionQueryCoordinator.hpp"
#include "Runtime/SessionFlow.hpp"
#include "Runtime/SessionPresentationCoordinator.hpp"
#include "Runtime/SessionRuntimeCoordinator.hpp"
#include "Runtime/TurnCoordinator.hpp"
#include "Runtime/TurnDraftCoordinator.hpp"
#include "Runtime/TurnLifecycleCoordinator.hpp"
#include "Runtime/UICallbackCoordinator.hpp"
#include "Runtime/UpdateCoordinator.hpp"
#include "Runtime/WeatherVisibility.hpp"
#include "Save/SaveManager.hpp"
#include "Systems/BuildOverlayRules.hpp"
#include "Systems/BuildSystem.hpp"
#include "Systems/CheckResponseRules.hpp"
#include "Systems/CheckSystem.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/InfernalSystem.hpp"
#include "Systems/PendingTurnProjection.hpp"
#include "Systems/ProductionSpawnRules.hpp"
#include "Systems/ProductionSystem.hpp"
#include "Systems/PublicBuildingOccupation.hpp"
#include "Systems/SelectionMoveRules.hpp"
#include "Systems/StructureIntegrityRules.hpp"
#include "Systems/TurnCommand.hpp"
#include "Systems/TurnSystem.hpp"
#include "Systems/WeatherSystem.hpp"
#include "Systems/XPSystem.hpp"
#include "UI/HUDLayout.hpp"
#include "UI/InGameViewModelBuilder.hpp"
#include "UI/MainMenuUI.hpp"
#include "UI/UIManager.hpp"
#include "UI/ToolBar.hpp"
#include "Units/MovementRules.hpp"
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
    barracks.cellBreachState.assign(barracks.width * barracks.height, 0);
    return barracks;
}

Building makeTestStoneWall(int id, KingdomId owner, const sf::Vector2i& origin, const GameConfig& config) {
    Building wall;
    wall.id = id;
    wall.type = BuildingType::StoneWall;
    wall.owner = owner;
    wall.isNeutral = false;
    wall.origin = origin;
    wall.width = config.getBuildingWidth(BuildingType::StoneWall);
    wall.height = config.getBuildingHeight(BuildingType::StoneWall);
    wall.cellHP.assign(wall.width * wall.height, config.getStoneWallHP());
    wall.cellBreachState.assign(wall.width * wall.height, 0);
    return wall;
}

Building makeTestPublicBuilding(BuildingType type, const sf::Vector2i& origin, int width, int height) {
    Building building;
    building.type = type;
    building.isNeutral = true;
    building.origin = origin;
    building.width = width;
    building.height = height;
    building.cellHP.assign(width * height, 1);
    building.cellBreachState.assign(width * height, 0);
    return building;
}

bool isBoardBorderCell(const Board& board, const sf::Vector2i& position) {
    if (!board.isInBounds(position.x, position.y) || !board.getCell(position.x, position.y).isInCircle) {
        return false;
    }

    static const int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& direction : directions) {
        const int nx = position.x + direction[0];
        const int ny = position.y + direction[1];
        if (!board.isInBounds(nx, ny) || !board.getCell(nx, ny).isInCircle) {
            return true;
        }
    }

    return false;
}

GameConfig makeInfernalTestConfig(const std::string& infernalJsonBody) {
    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "anormalchess_infernal_test_config.json";
    {
        std::ofstream out(tempPath);
        out << "{\n"
            << "  \"game\": {\n"
            << "    \"infernal\": {\n"
            << infernalJsonBody << "\n"
            << "    }\n"
            << "  }\n"
            << "}\n";
    }

    GameConfig config;
    expect(config.loadFromFile(tempPath.string()),
        "Infernal test config helper should load a temporary infernal override file.");
    std::filesystem::remove(tempPath);
    return config;
}

GameConfig makeXPTestConfig(const std::string& xpJsonBody) {
    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "anormalchess_xp_test_config.json";
    {
        std::ofstream out(tempPath);
        out << "{\n"
            << "  \"game\": {\n"
            << "    \"xp\": {\n"
            << xpJsonBody << "\n"
            << "    }\n"
            << "  }\n"
            << "}\n";
    }

    GameConfig config;
    expect(config.loadFromFile(tempPath.string()),
        "XP test config helper should load a temporary XP override file.");
    std::filesystem::remove(tempPath);
    return config;
}

GameConfig makeWeatherTestConfig(const std::string& weatherJsonBody) {
    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "anormalchess_weather_test_config.json";
    {
        std::ofstream out(tempPath);
        out << "{\n"
            << "  \"game\": {\n"
            << "    \"weather\": {\n"
            << weatherJsonBody << "\n"
            << "    }\n"
            << "  }\n"
            << "}\n";
    }

    GameConfig config;
    expect(config.loadFromFile(tempPath.string()),
        "Weather test config helper should load a temporary weather override file.");
    std::filesystem::remove(tempPath);
    return config;
}

Piece& addPieceToBoard(Kingdom& kingdom,
                       Board& board,
                       int id,
                       PieceType type,
                       KingdomId owner,
                       const sf::Vector2i& position) {
    kingdom.addPiece(Piece(id, type, owner, position));
    Piece& piece = kingdom.pieces.back();
    board.getCell(position.x, position.y).piece = &piece;
    return piece;
}

AutonomousUnit makeTestInfernalUnit(int id,
                                    KingdomId targetKingdom,
                                    const sf::Vector2i& position,
                                    PieceType manifestedPieceType = PieceType::Queen) {
    AutonomousUnit unit;
    unit.id = id;
    unit.type = AutonomousUnitType::InfernalPiece;
    unit.position = position;
    unit.infernal.targetKingdom = targetKingdom;
    unit.infernal.targetPieceId = -1;
    unit.infernal.manifestedPieceType = manifestedPieceType;
    unit.infernal.preferredTargetType = PieceType::Queen;
    unit.infernal.phase = InfernalPhase::Hunting;
    unit.infernal.returnBorderCell = {0, position.y};
    unit.infernal.spawnTurn = 1;
    return unit;
}

Building* findBuildingById(Kingdom& kingdom, int buildingId) {
    for (Building& building : kingdom.buildings) {
        if (building.id == buildingId) {
            return &building;
        }
    }

    return nullptr;
}

void linkBuildingOnBoard(Building& building, Board& board) {
    for (const sf::Vector2i& pos : building.getOccupiedCells()) {
        board.getCell(pos.x, pos.y).building = &building;
    }
}

int ringDistanceFromBuilding(const Building& building, const sf::Vector2i& position) {
    const int left = building.origin.x;
    const int top = building.origin.y;
    const int right = building.origin.x + building.getFootprintWidth() - 1;
    const int bottom = building.origin.y + building.getFootprintHeight() - 1;

    const int dx = (position.x < left)
        ? (left - position.x)
        : (position.x > right ? position.x - right : 0);
    const int dy = (position.y < top)
        ? (top - position.y)
        : (position.y > bottom ? position.y - bottom : 0);
    return std::max(dx, dy);
}

float buildingCenterDistance(const Building& lhs, const Building& rhs) {
    const float lhsCenterX = static_cast<float>(lhs.origin.x) + (static_cast<float>(lhs.getFootprintWidth()) * 0.5f);
    const float lhsCenterY = static_cast<float>(lhs.origin.y) + (static_cast<float>(lhs.getFootprintHeight()) * 0.5f);
    const float rhsCenterX = static_cast<float>(rhs.origin.x) + (static_cast<float>(rhs.getFootprintWidth()) * 0.5f);
    const float rhsCenterY = static_cast<float>(rhs.origin.y) + (static_cast<float>(rhs.getFootprintHeight()) * 0.5f);
    const float dx = lhsCenterX - rhsCenterX;
    const float dy = lhsCenterY - rhsCenterY;
    return std::sqrt((dx * dx) + (dy * dy));
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

bool snapshotHasAnyLegalResponse(const Board& board,
                                 const Kingdom& activeKingdom,
                                 const Kingdom& enemyKingdom,
                                 const std::vector<Building>& publicBuildings,
                                 int turnNumber,
                                 const GameConfig& config) {
    const GameSnapshot snapshot = ForwardModel::createSnapshot(
        board, activeKingdom, enemyKingdom, publicBuildings, turnNumber);
    for (const SnapPiece& piece : snapshot.kingdom(activeKingdom.id).pieces) {
        if (!ForwardModel::getLegalMoves(snapshot, piece, config.getGlobalMaxRange()).empty()) {
            return true;
        }
    }

    return false;
}

bool containsCell(const std::vector<sf::Vector2i>& cells,
                  const sf::Vector2i& target) {
    return std::find(cells.begin(), cells.end(), target) != cells.end();
}

bool sameCellSet(std::vector<sf::Vector2i> lhs,
                 std::vector<sf::Vector2i> rhs) {
    auto order = [](const sf::Vector2i& a, const sf::Vector2i& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    };
    std::sort(lhs.begin(), lhs.end(), order);
    lhs.erase(std::unique(lhs.begin(), lhs.end()), lhs.end());
    std::sort(rhs.begin(), rhs.end(), order);
    rhs.erase(std::unique(rhs.begin(), rhs.end()), rhs.end());
    return lhs == rhs;
}

void expectVec2i(sf::Vector2i actual,
                 sf::Vector2i expected,
                 const std::string& message) {
    expect(actual == expected,
        message + " Expected {" + std::to_string(expected.x) + ", " + std::to_string(expected.y)
        + "}, got {" + std::to_string(actual.x) + ", " + std::to_string(actual.y) + "}.");
}

TurnCommand makeMoveCommand(int pieceId,
                            const sf::Vector2i& origin,
                            const sf::Vector2i& destination) {
    TurnCommand command;
    command.type = TurnCommand::Move;
    command.pieceId = pieceId;
    command.origin = origin;
    command.destination = destination;
    return command;
}

TurnCommand makeBuildCommand(BuildingType type,
                             const sf::Vector2i& origin,
                             int rotationQuarterTurns = 0) {
    TurnCommand command;
    command.type = TurnCommand::Build;
    command.buildingType = type;
    command.buildOrigin = origin;
    command.buildRotationQuarterTurns = rotationQuarterTurns;
    return command;
}

TurnCommand makeDisbandCommand(int pieceId) {
    TurnCommand command;
    command.type = TurnCommand::Disband;
    command.pieceId = pieceId;
    return command;
}

InputContext makePassiveInputContext(sf::RenderWindow& window,
                                     Camera& camera,
                                     Board& board,
                                     TurnSystem& turnSystem,
                                     BuildingFactory& buildingFactory,
                                     Kingdom& controlledKingdom,
                                     Kingdom& opposingKingdom,
                                     std::vector<Building>& publicBuildings,
                                     UIManager& uiManager,
                                     const GameConfig& config) {
    return InputContext{
        window,
        camera,
        board,
        turnSystem,
        buildingFactory,
        controlledKingdom,
        opposingKingdom,
        publicBuildings,
        board,
        controlledKingdom,
        opposingKingdom,
        publicBuildings,
        TurnValidationContext{board,
                              controlledKingdom,
                              opposingKingdom,
                              publicBuildings,
                              turnSystem.getTurnNumber(),
                              config},
        uiManager,
        config,
                    nullptr,
                    controlledKingdom.id,
        InteractionPermissions{},
        false,
        false};
}

InGameHudPresentation makeHudPresentation(
    KingdomId statsKingdom,
    bool showTurnPointIndicators = true,
    InGameTurnIndicatorTone turnIndicatorTone = InGameTurnIndicatorTone::Neutral) {
    InGameHudPresentation presentation;
    presentation.statsKingdom = statsKingdom;
    presentation.showTurnPointIndicators = showTurnPointIndicators;
    presentation.turnIndicatorTone = turnIndicatorTone;
    return presentation;
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
            signature.push_back(':');
            signature += std::to_string(static_cast<int>(cell.terrainBrightness));
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

void testLayeredSelectionStackTreatsUnderConstructionBuildingAsNormalBuilding() {
    Cell cell;
    cell.type = CellType::Grass;
    cell.isInCircle = true;

    Building building;
    building.type = BuildingType::Barracks;
    building.setConstructionState(BuildingState::UnderConstruction);
    cell.building = &building;

    const LayeredSelectionStack stack = resolveCellSelectionStack(cell, {3, 3});
    expect(stack.count == 2,
        "Under-construction buildings should still use the normal building plus terrain selection stack.");
    expect(stack.top() == SelectionLayer::Building,
        "Under-construction buildings should be selected through the normal building layer.");
    expect(stack.nextBelow(SelectionLayer::Building) == SelectionLayer::Terrain,
        "Cycling below an under-construction building should expose terrain next.");
}

void testInputHandlerBookmarksActivePieceSelectionCell() {
    GameConfig config;
    Board board;
    board.init(12);

    Kingdom white(KingdomId::White);
    Kingdom black(KingdomId::Black);
    Piece& pawn = addPieceToBoard(white, board, 1101, PieceType::Pawn, KingdomId::White, {4, 5});
    addPieceToBoard(black, board, 2101, PieceType::King, KingdomId::Black, {9, 9});

    TurnSystem turnSystem;
    turnSystem.setActiveKingdom(KingdomId::White);
    turnSystem.setTurnNumber(1);
    BuildingFactory buildingFactory;
    std::vector<Building> publicBuildings;
    sf::RenderWindow window;
    Camera camera;
    UIManager uiManager;
    InputContext context = makePassiveInputContext(window,
                                                   camera,
                                                   board,
                                                   turnSystem,
                                                   buildingFactory,
                                                   white,
                                                   black,
                                                   publicBuildings,
                                                   uiManager,
                                                   config);
    context.permissions.canIssueCommands = false;

    InputHandler input;
    input.reconcileSelection(InputSelectionBookmark{}, &pawn, nullptr, context);

    const InputSelectionBookmark bookmark = input.createSelectionBookmark();
    expect(bookmark.pieceId == pawn.id,
        "InputHandler should preserve the selected piece id when creating a bookmark for a piece selection.");
    expect(bookmark.selectedCell.has_value() && *bookmark.selectedCell == sf::Vector2i{4, 5},
        "InputHandler should preserve the active piece selection cell in bookmarks so draft reconciliation can restore the original clicked location.");
}

void testInputHandlerReconcileSelectionDoesNotJumpToMismatchedPieceElsewhere() {
    GameConfig config;
    Board initialBoard;
    initialBoard.init(12);
    const sf::Vector2i bookmarkedCell{4, 5};
    const sf::Vector2i movedCell{4, 6};

    Kingdom initialWhite(KingdomId::White);
    Kingdom initialBlack(KingdomId::Black);
    Piece& initialPawn = addPieceToBoard(initialWhite,
                                         initialBoard,
                                         1102,
                                         PieceType::Pawn,
                                         KingdomId::White,
                                         bookmarkedCell);
    addPieceToBoard(initialBlack, initialBoard, 2102, PieceType::King, KingdomId::Black, {9, 9});

    TurnSystem initialTurnSystem;
    initialTurnSystem.setActiveKingdom(KingdomId::White);
    initialTurnSystem.setTurnNumber(1);
    BuildingFactory initialBuildingFactory;
    std::vector<Building> initialPublicBuildings;
    sf::RenderWindow initialWindow;
    Camera initialCamera;
    UIManager initialUiManager;
    InputContext initialContext = makePassiveInputContext(initialWindow,
                                                          initialCamera,
                                                          initialBoard,
                                                          initialTurnSystem,
                                                          initialBuildingFactory,
                                                          initialWhite,
                                                          initialBlack,
                                                          initialPublicBuildings,
                                                          initialUiManager,
                                                          config);
    initialContext.permissions.canIssueCommands = false;

    InputHandler input;
    input.reconcileSelection(InputSelectionBookmark{}, &initialPawn, nullptr, initialContext);
    const InputSelectionBookmark bookmark = input.createSelectionBookmark();

    Board driftBoard;
    driftBoard.init(12);
    Kingdom driftWhite(KingdomId::White);
    Kingdom driftBlack(KingdomId::Black);
    addPieceToBoard(driftWhite,
                    driftBoard,
                    1103,
                    PieceType::Rook,
                    KingdomId::White,
                    bookmarkedCell);
    addPieceToBoard(driftWhite,
                    driftBoard,
                    1102,
                    PieceType::Pawn,
                    KingdomId::White,
                    movedCell);
    addPieceToBoard(driftBlack, driftBoard, 2103, PieceType::King, KingdomId::Black, {9, 9});
    Piece* movedPawn = nullptr;
    for (Piece& piece : driftWhite.pieces) {
        if (piece.id == 1102) {
            movedPawn = &piece;
            break;
        }
    }

    TurnSystem driftTurnSystem;
    driftTurnSystem.setActiveKingdom(KingdomId::White);
    driftTurnSystem.setTurnNumber(1);
    BuildingFactory driftBuildingFactory;
    std::vector<Building> driftPublicBuildings;
    sf::RenderWindow driftWindow;
    Camera driftCamera;
    UIManager driftUiManager;
    InputContext driftContext = makePassiveInputContext(driftWindow,
                                                        driftCamera,
                                                        driftBoard,
                                                        driftTurnSystem,
                                                        driftBuildingFactory,
                                                        driftWhite,
                                                        driftBlack,
                                                        driftPublicBuildings,
                                                        driftUiManager,
                                                        config);
    driftContext.permissions.canIssueCommands = false;

    expect(movedPawn != nullptr,
        "The drift setup should expose the moved same-id pawn so reconciliation can be tested against the stale id-based fallback path.");

    input.reconcileSelection(bookmark, movedPawn, nullptr, driftContext);

    expect(input.getSelectedPieceId() == -1,
        "InputHandler should not jump selection to a same-id piece resolved elsewhere when the bookmarked cell no longer contains the originally selected piece.");
    expect(input.hasSelectedCell() && input.getSelectedCell() == bookmarkedCell,
        "InputHandler should preserve the bookmarked world cell as a terrain selection instead of selecting a distant piece after reconciliation drift.");
}

void testInputHandlerReconcileSelectionPrefersVisiblePieceOverPendingMoveOverride() {
    GameConfig config;
    const sf::Vector2i bookmarkedCell{4, 2};

    Board initialBoard;
    initialBoard.init(12);
    Kingdom initialWhite(KingdomId::White);
    Kingdom initialBlack(KingdomId::Black);
    addPieceToBoard(initialWhite, initialBoard, 1200, PieceType::King, KingdomId::White, {1, 1});
    addPieceToBoard(initialWhite, initialBoard, 1201, PieceType::Rook, KingdomId::White, {2, 2});
    Piece& initialBlackPawn = addPieceToBoard(initialBlack,
                                              initialBoard,
                                              2201,
                                              PieceType::Pawn,
                                              KingdomId::Black,
                                              bookmarkedCell);
    addPieceToBoard(initialBlack, initialBoard, 2200, PieceType::King, KingdomId::Black, {9, 9});

    TurnSystem initialTurnSystem;
    initialTurnSystem.setActiveKingdom(KingdomId::White);
    initialTurnSystem.setTurnNumber(1);
    BuildingFactory initialBuildingFactory;
    std::vector<Building> initialPublicBuildings;
    sf::RenderWindow initialWindow;
    Camera initialCamera;
    UIManager initialUiManager;
    InputContext initialContext = makePassiveInputContext(initialWindow,
                                                          initialCamera,
                                                          initialBoard,
                                                          initialTurnSystem,
                                                          initialBuildingFactory,
                                                          initialWhite,
                                                          initialBlack,
                                                          initialPublicBuildings,
                                                          initialUiManager,
                                                          config);
    initialContext.permissions.canIssueCommands = false;

    InputHandler input;
    input.reconcileSelection(InputSelectionBookmark{}, &initialBlackPawn, nullptr, initialContext);
    const InputSelectionBookmark bookmark = input.createSelectionBookmark();

    Board previewBoard;
    previewBoard.init(12);
    Kingdom previewWhite(KingdomId::White);
    Kingdom previewBlack(KingdomId::Black);
    addPieceToBoard(previewWhite, previewBoard, 1200, PieceType::King, KingdomId::White, {1, 1});
    addPieceToBoard(previewWhite, previewBoard, 1201, PieceType::Rook, KingdomId::White, {2, 2});
    Piece& previewBlackPawn = addPieceToBoard(previewBlack,
                                              previewBoard,
                                              2201,
                                              PieceType::Pawn,
                                              KingdomId::Black,
                                              bookmarkedCell);
    addPieceToBoard(previewBlack, previewBoard, 2200, PieceType::King, KingdomId::Black, {9, 9});

    TurnSystem previewTurnSystem;
    previewTurnSystem.setActiveKingdom(KingdomId::White);
    previewTurnSystem.setTurnNumber(1);
    std::vector<Building> previewPublicBuildings;
    TurnCommand pendingCapture = makeMoveCommand(1201, {2, 2}, bookmarkedCell);
    expect(previewTurnSystem.queueCommand(pendingCapture,
                                          previewBoard,
                                          previewWhite,
                                          previewBlack,
                                          previewPublicBuildings,
                                          config),
        "The pending override regression test should queue a legal capture so the preview resolver sees a move destination at the bookmarked cell.");

    BuildingFactory previewBuildingFactory;
    sf::RenderWindow previewWindow;
    Camera previewCamera;
    UIManager previewUiManager;
    InputContext previewContext = makePassiveInputContext(previewWindow,
                                                          previewCamera,
                                                          previewBoard,
                                                          previewTurnSystem,
                                                          previewBuildingFactory,
                                                          previewWhite,
                                                          previewBlack,
                                                          previewPublicBuildings,
                                                          previewUiManager,
                                                          config);
    previewContext.permissions.canIssueCommands = false;

    input.reconcileSelection(bookmark, &previewBlackPawn, nullptr, previewContext);

    const InputSelectionBookmark restoredBookmark = input.createSelectionBookmark();
    expect(input.getSelectedPieceId() == previewBlackPawn.id,
        "InputHandler should keep the visible piece selected when a pending move also targets the clicked cell in a non-concrete preview state.");
    expect(restoredBookmark.selectedCell.has_value() && *restoredBookmark.selectedCell == bookmarkedCell,
        "InputHandler should preserve the bookmarked cell anchor instead of demoting the selection when a visible piece still occupies that cell.");
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

    void testCellTraversalTreatsWaterAsNotTraversable() {
        Cell waterCell;
        waterCell.type = CellType::Water;
        waterCell.isInCircle = true;
        waterCell.position = {2, 2};

        expect(!isCellTerrainTraversable(waterCell),
            "Water cells inside the map radius should still be reported as non-traversable.");
    }

    void testWeatherVisibilityKeepsAlliesVisibleAndConcealsEnemies() {
        GameConfig config;

        WeatherMaskCache weatherMaskCache;
        weatherMaskCache.hasActiveFront = true;
        weatherMaskCache.diameter = 8;
        weatherMaskCache.alphaByCell.assign(
            static_cast<std::size_t>(weatherMaskCache.diameter * weatherMaskCache.diameter),
            0);
        weatherMaskCache.shadeByCell.assign(
            static_cast<std::size_t>(weatherMaskCache.diameter * weatherMaskCache.diameter),
            0);
        weatherMaskCache.alphaByCell[static_cast<std::size_t>((3 * weatherMaskCache.diameter) + 3)] = 255;

        const Building alliedBarracks = makeTestBarracks(90, KingdomId::White, {3, 3}, config);
        const Building enemyBarracks = makeTestBarracks(91, KingdomId::Black, {3, 3}, config);
        const Piece alliedPawn(92, PieceType::Pawn, KingdomId::White, {3, 3});
        const Piece enemyPawn(93, PieceType::Pawn, KingdomId::Black, {3, 3});

        expect(!WeatherVisibility::shouldHideBuildingCell(alliedBarracks,
                                                          {3, 3},
                                                          KingdomId::White,
                                                          weatherMaskCache),
            "Fog should not hide allied building cells for the local kingdom.");
        expect(WeatherVisibility::shouldHideBuildingCell(enemyBarracks,
                                                         {3, 3},
                                                         KingdomId::White,
                                                         weatherMaskCache),
            "Fog should fully hide enemy building cells for the local kingdom.");
        expect(!WeatherVisibility::shouldHideBuildingOverlay(alliedBarracks,
                                                             KingdomId::White,
                                                             weatherMaskCache),
            "Fog should not suppress allied building overlays for the local kingdom.");
        expect(WeatherVisibility::shouldHideBuildingOverlay(enemyBarracks,
                                                            KingdomId::White,
                                                            weatherMaskCache),
            "Fog should suppress enemy building overlays when any covered enemy cell would leak their presence.");
        expect(!WeatherVisibility::shouldHidePiece(alliedPawn,
                                                   KingdomId::White,
                                                   weatherMaskCache),
            "Fog should not hide allied pieces for the local kingdom.");
        expect(WeatherVisibility::shouldHidePiece(enemyPawn,
                                                  KingdomId::White,
                                                  weatherMaskCache),
            "Fog should hide enemy pieces when their cell is covered.");
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

    void testUnderConstructionStructureOverlayStacksConstructionIconInPrimaryRow() {
        GameConfig config;
        Board board;
        board.init(5);

        Building barracks = makeTestBarracks(11, KingdomId::White, {1, 1}, config);
        barracks.setConstructionState(BuildingState::UnderConstruction);

        StructureOverlayContext overlayContext;
        overlayContext.isSelected = false;
        const StructureOverlayStack overlay = buildStructureOverlay(
            barracks, board, config, overlayContext, makeWorldStructureOverlayPolicy());

        expect(overlay.rows.size() == 1,
            "Under-construction private buildings should keep a single primary status row above the structure.");
        expect(overlay.rows[0].placement == StructureOverlayRowPlacement::Above,
            "The under-construction status indicators should remain in the primary row above the structure.");
        expect(overlay.rows[0].items.size() == 2,
            "Under-construction private buildings should stack the owner and construction indicators side by side in one row.");
        expect(overlay.rows[0].items[0].type == StructureOverlayItemType::Icon,
            "The primary under-construction row should begin with the owner icon.");
        expect(overlay.rows[0].items[0].icon.textureName == "shield_white",
            "The owner indicator should remain the first item in the primary under-construction row.");
        expect(overlay.rows[0].items[1].type == StructureOverlayItemType::Icon,
            "The construction indicator should be added as a second icon in the primary row.");
        expect(overlay.rows[0].items[1].icon.textureName == "build_ongoing",
            "Under-construction buildings should expose the hammer icon in the same primary row as other status indicators.");
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

    void testCheckResponseFiltersMovesThatDoNotResolveCheck() {
        GameConfig config;
        Board board;
        board.init(8);
        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);

        Piece& whiteKing = addPieceToBoard(white, board, 100, PieceType::King, KingdomId::White, {8, 8});
        (void)whiteKing;
        Piece& whiteKnight = addPieceToBoard(white, board, 101, PieceType::Knight, KingdomId::White, {4, 7});
        Piece& blackRook = addPieceToBoard(black, board, 200, PieceType::Rook, KingdomId::Black, {8, 4});
        (void)blackRook;

        const std::vector<sf::Vector2i> legalMoves = CheckResponseRules::filterLegalMovesForPiece(
            whiteKnight, board, config);
        expect(legalMoves.empty(),
               "A non-king piece should have no selectable moves while in check if none of its moves resolve the check.");
    }

    void testCheckResponseAllowsBlockingMove() {
        GameConfig config;
        Board board;
        board.init(8);
        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);

        addPieceToBoard(white, board, 110, PieceType::King, KingdomId::White, {8, 8});
        Piece& whiteBishop = addPieceToBoard(white, board, 111, PieceType::Bishop, KingdomId::White, {7, 7});
        addPieceToBoard(black, board, 210, PieceType::Rook, KingdomId::Black, {8, 4});

        const std::vector<sf::Vector2i> legalMoves = CheckResponseRules::filterLegalMovesForPiece(
            whiteBishop, board, config);
        expect(std::find(legalMoves.begin(), legalMoves.end(), sf::Vector2i{8, 6}) != legalMoves.end(),
               "A friendly piece should keep the blocking move that resolves a line check.");
    }

    void testCheckResponseRejectsPassWhileInCheck() {
        GameConfig config;
        Board board;
        board.init(8);
        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);

        addPieceToBoard(white, board, 120, PieceType::King, KingdomId::White, {8, 8});
        addPieceToBoard(black, board, 220, PieceType::Rook, KingdomId::Black, {8, 4});

        const std::vector<Building> publicBuildings;
        const CheckTurnValidation validation = CheckResponseRules::validatePendingTurn(
            white, black, board, publicBuildings, 1, {}, config);
        expect(!validation.valid && validation.activeKingInCheck,
               "An empty pending turn should be rejected while the kingdom is in check.");
    }

    void testCheckResponseRejectsNonMoveActionsWhileInCheck() {
        GameConfig config;
        Board board;
        board.init(8);
        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);

        addPieceToBoard(white, board, 130, PieceType::King, KingdomId::White, {8, 8});
        addPieceToBoard(black, board, 230, PieceType::Rook, KingdomId::Black, {8, 4});

        TurnCommand buildCommand;
        buildCommand.type = TurnCommand::Build;
        buildCommand.buildingType = BuildingType::Barracks;
        buildCommand.buildOrigin = {7, 8};

        const std::vector<Building> publicBuildings;
        const CheckTurnValidation validation = CheckResponseRules::validatePendingTurn(
            white, black, board, publicBuildings, 1, {buildCommand}, config);
        expect(!validation.valid,
               "Build and other non-move commands must be rejected while the active kingdom is in check.");
    }

    void testSelectionMoveRulesClassifyUnsafeNonKingMovesAsSelectable() {
        GameConfig config;
        Board board;
        board.init(12);
        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);

        addPieceToBoard(white, board, 310, PieceType::King, KingdomId::White, {16, 16});
        addPieceToBoard(white, board, 311, PieceType::Rook, KingdomId::White, {10, 16});
        addPieceToBoard(black, board, 410, PieceType::King, KingdomId::Black, {10, 10});
        Piece& blackRook = addPieceToBoard(black, board, 411, PieceType::Rook, KingdomId::Black, {10, 12});

        const std::vector<Building> publicBuildings;
        const SelectionMoveOptions moveOptions = SelectionMoveRules::classifyPieceMoves(
            board, black, white, publicBuildings, 1, {}, blackRook.id, config);

        expect(containsCell(moveOptions.safeMoves, {10, 13}),
            "A shielding rook should keep safe moves that continue protecting its king.");
        expect(containsCell(moveOptions.unsafeMoves, {9, 12}),
            "Unsafe self-check moves should still be exposed as red selectable destinations.");
        expect(moveOptions.contains({9, 12}),
            "Selection move options should treat unsafe red destinations as selectable.");
    }

    void testSelectionMoveRulesKeepUnsafeKingSquaresSelectable() {
        GameConfig config;
        Board board;
        board.init(12);
        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);

        addPieceToBoard(white, board, 320, PieceType::King, KingdomId::White, {4, 4});
        addPieceToBoard(white, board, 321, PieceType::Rook, KingdomId::White, {10, 12});
        Piece& blackKing = addPieceToBoard(black, board, 420, PieceType::King, KingdomId::Black, {10, 10});

        const std::vector<Building> publicBuildings;
        const SelectionMoveOptions moveOptions = SelectionMoveRules::classifyPieceMoves(
            board, black, white, publicBuildings, 1, {}, blackKing.id, config);

        expect(containsCell(moveOptions.safeMoves, {9, 10}),
            "The king should keep safe escape squares in the selection model.");
        expect(containsCell(moveOptions.unsafeMoves, {10, 11}),
            "King squares that remain under attack should stay classified as red destinations.");
        expect(moveOptions.contains({10, 11}),
            "Red king destinations should remain selectable even though they are unsafe.");
    }

    void testSelectionMoveRulesIgnoreQueuedUpgradeForLivePieceMoves() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        Piece& pawn = addPieceToBoard(white, board, 322, PieceType::Pawn, KingdomId::White, {8, 8});
        addPieceToBoard(white, board, 323, PieceType::King, KingdomId::White, {4, 4});
        addPieceToBoard(black, board, 422, PieceType::King, KingdomId::Black, {16, 16});
        pawn.xp = config.getXPThresholdPawnToKnightOrBishop();
        white.gold = config.getUpgradeCost(PieceType::Pawn, PieceType::Bishop) + 5;

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);

        TurnCommand upgradeCommand;
        upgradeCommand.type = TurnCommand::Upgrade;
        upgradeCommand.upgradePieceId = pawn.id;
        upgradeCommand.upgradeTarget = PieceType::Bishop;
        expect(turnSystem.queueCommand(upgradeCommand, board, white, black, publicBuildings, config),
            "Eligible upgrades should queue successfully for live move filtering coverage.");

        const SelectionMoveOptions moveOptions = SelectionMoveRules::classifyPieceMoves(
            board,
            white,
            black,
            publicBuildings,
            1,
            turnSystem.getPendingCommands(),
            pawn.id,
            config);

        expect(containsCell(moveOptions.safeMoves, {8, 7}),
            "Queueing an upgrade must not remove the pawn's current orthogonal live move before commit.");
        expect(!containsCell(moveOptions.safeMoves, {7, 7}) && !containsCell(moveOptions.unsafeMoves, {7, 7}),
            "Queueing an upgrade must not grant bishop-style diagonal live moves before commit.");
    }

    void testTurnSystemAllowsMoveAfterQueuedUpgradeBeforeCommit() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        Piece& pawn = addPieceToBoard(white, board, 324, PieceType::Pawn, KingdomId::White, {8, 8});
        addPieceToBoard(white, board, 325, PieceType::King, KingdomId::White, {4, 4});
        addPieceToBoard(black, board, 423, PieceType::King, KingdomId::Black, {16, 16});
        pawn.xp = config.getXPThresholdPawnToKnightOrBishop();
        const int upgradeCost = config.getUpgradeCost(PieceType::Pawn, PieceType::Bishop);
        const int startingGold = upgradeCost + 5;
        white.gold = startingGold;

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);

        TurnCommand upgradeCommand;
        upgradeCommand.type = TurnCommand::Upgrade;
        upgradeCommand.upgradePieceId = pawn.id;
        upgradeCommand.upgradeTarget = PieceType::Bishop;
        expect(turnSystem.queueCommand(upgradeCommand, board, white, black, publicBuildings, config),
            "Eligible upgrades should queue successfully before testing deferred transformation semantics.");

        TurnCommand moveCommand;
        moveCommand.type = TurnCommand::Move;
        moveCommand.pieceId = pawn.id;
        moveCommand.origin = pawn.position;
        moveCommand.destination = {8, 7};
        expect(turnSystem.queueCommand(moveCommand, board, white, black, publicBuildings, config),
            "A queued upgrade must not prevent the piece from queuing a legal move under its current type before commit.");

        EventLog eventLog;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;
        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        Piece* upgradedPiece = white.getPieceById(pawn.id);
        expect(upgradedPiece != nullptr, "The upgraded piece should still exist after commit.");
        expect(upgradedPiece->position == sf::Vector2i(8, 7),
            "The queued move should resolve before the deferred upgrade is applied at commit.");
        expect(upgradedPiece->type == PieceType::Bishop,
            "The queued upgrade should still apply when the turn is validated.");
        expect(board.getCell(8, 7).piece == upgradedPiece,
            "The destination cell should contain the moved piece after commit.");
        const int expectedEndingGold = std::max(
            0,
            startingGold
                - upgradeCost
                - config.getPieceUpkeepCost(PieceType::Bishop)
                - config.getPieceUpkeepCost(PieceType::King));
        expect(white.gold == expectedEndingGold,
            "Deferred upgrades should still spend their cost before the normal end-of-turn economy resolves.");
    }

    void testHudLayoutKeepsNetIncomeWide() {
        expect(HUDLayout::metricWidths()[3] == HUDLayout::kWideMetricWidth,
            "Net Income should use the wide metric slot so its label fits without overflow.");
    }

    void testToolBarPresentationTracksSwitcherStates() {
        const ToolBarPresentation selectOverviewVisible = makeToolBarPresentation(
            ToolState::Select,
            true,
            true,
            true);
        expect(selectOverviewVisible.selectSelected,
            "The Select toolbar button should appear active while the select tool is selected.");
        expect(!selectOverviewVisible.buildSelected,
            "The Build toolbar button should not appear active while the select tool is selected.");
        expect(selectOverviewVisible.buildEnabled,
            "The Build toolbar button should stay enabled when the build panel is available.");
        expect(selectOverviewVisible.overviewSelected,
            "The Overview toolbar button should appear active while the right sidebar is visible.");
        expect(selectOverviewVisible.overviewEnabled,
            "The Overview toolbar button should stay enabled whenever the toolbar itself is usable.");

        const ToolBarPresentation buildOverviewHidden = makeToolBarPresentation(
            ToolState::Build,
            true,
            false,
            false);
        expect(!buildOverviewHidden.selectSelected,
            "The Select toolbar button should not appear active while the build tool is selected.");
        expect(buildOverviewHidden.buildSelected,
            "The Build toolbar button should appear active while the build tool is selected.");
        expect(!buildOverviewHidden.buildEnabled,
            "The Build toolbar button should reflect when the build panel is temporarily unavailable.");
        expect(!buildOverviewHidden.overviewSelected,
            "The Overview toolbar button should appear inactive while the right sidebar is hidden.");
    }

    void testPendingTurnValidationRejectsUnsafeQueuedMoveOnlyAtEndTurn() {
        GameConfig config;
        Board board;
        board.init(12);
        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);

        addPieceToBoard(white, board, 330, PieceType::King, KingdomId::White, {16, 16});
        addPieceToBoard(white, board, 331, PieceType::Rook, KingdomId::White, {10, 16});
        addPieceToBoard(black, board, 430, PieceType::King, KingdomId::Black, {10, 10});
        Piece& blackRook = addPieceToBoard(black, board, 431, PieceType::Rook, KingdomId::Black, {10, 12});

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::Black);

        TurnCommand moveCommand;
        moveCommand.type = TurnCommand::Move;
        moveCommand.pieceId = blackRook.id;
        moveCommand.origin = blackRook.position;
        moveCommand.destination = {9, 12};

        expect(turnSystem.queueCommand(moveCommand, board, black, white, publicBuildings, config),
            "Unsafe self-check moves should still be queueable so the player can preview them.");

        const CheckTurnValidation validation = CheckResponseRules::validatePendingTurn(
            black, white, board, publicBuildings, 1, turnSystem.getPendingCommands(), config);
        expect(!validation.valid,
            "The queued turn must stay invalid until the player resolves a red self-check move.");
        expect(validation.hasQueuedMove,
            "Unsafe queued moves should remain present for end-turn validation.");
    }

    void testAutomaticChurchCoronationTurnsFirstRookIntoQueen() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        std::vector<Building> publicBuildings;
        publicBuildings.push_back(makeTestPublicBuilding(BuildingType::Church, {10, 10}, 4, 3));
        linkBuildingOnBoard(publicBuildings.back(), board);

        addPieceToBoard(white, board, 500, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(white, board, 501, PieceType::Bishop, KingdomId::White, {11, 10});
        addPieceToBoard(white, board, 502, PieceType::Rook, KingdomId::White, {12, 10});
        addPieceToBoard(white, board, 503, PieceType::Rook, KingdomId::White, {10, 11});

        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        EventLog eventLog;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;
        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        const Piece* firstRook = white.getPieceById(502);
        const Piece* secondRook = white.getPieceById(503);
        expect(firstRook && firstRook->type == PieceType::Queen,
            "The first rook found on the church should become a queen at turn commit.");
        expect(secondRook && secondRook->type == PieceType::Rook,
            "Only one rook should coronate per turn when multiple rooks stand in the church.");
    }

    void testAutomaticChurchCoronationAllowsMultipleQueens() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        std::vector<Building> publicBuildings;
        publicBuildings.push_back(makeTestPublicBuilding(BuildingType::Church, {10, 10}, 4, 3));
        linkBuildingOnBoard(publicBuildings.back(), board);

        addPieceToBoard(white, board, 510, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(white, board, 511, PieceType::Bishop, KingdomId::White, {11, 10});
        addPieceToBoard(white, board, 512, PieceType::Rook, KingdomId::White, {12, 10});
        addPieceToBoard(white, board, 513, PieceType::Queen, KingdomId::White, {6, 6});

        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        EventLog eventLog;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;
        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        int queenCount = 0;
        for (const Piece& piece : white.pieces) {
            if (piece.type == PieceType::Queen) {
                ++queenCount;
            }
        }

        expect(queenCount == 2,
            "Church coronation should still happen when the kingdom already owns a queen.");
    }

    void testAutomaticChurchCoronationBlockedByEnemyPresence() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        std::vector<Building> publicBuildings;
        publicBuildings.push_back(makeTestPublicBuilding(BuildingType::Church, {10, 10}, 4, 3));
        linkBuildingOnBoard(publicBuildings.back(), board);

        addPieceToBoard(white, board, 520, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(white, board, 521, PieceType::Bishop, KingdomId::White, {11, 10});
        addPieceToBoard(white, board, 522, PieceType::Rook, KingdomId::White, {12, 10});
        addPieceToBoard(black, board, 620, PieceType::Pawn, KingdomId::Black, {13, 10});

        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        EventLog eventLog;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;
        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        const Piece* rook = white.getPieceById(522);
        expect(rook && rook->type == PieceType::Rook,
            "Enemy presence in the church should block automatic coronation.");
    }

    void testPawnMovesOrthogonallyAndCapturesDiagonally() {
        GameConfig config;
        Board board;
        board.init(8);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        Piece& pawn = addPieceToBoard(white, board, 700, PieceType::Pawn, KingdomId::White, {8, 8});
        addPieceToBoard(black, board, 701, PieceType::Knight, KingdomId::Black, {9, 9});
        addPieceToBoard(black, board, 702, PieceType::Knight, KingdomId::Black, {8, 9});

        const std::vector<sf::Vector2i> validMoves = MovementRules::getValidMoves(pawn, board, config);
        expect(containsCell(validMoves, {8, 7}),
            "Pawns should still move orthogonally onto open squares.");
        expect(!containsCell(validMoves, {8, 9}),
            "Pawns should no longer capture enemy pieces orthogonally.");
        expect(containsCell(validMoves, {9, 9}),
            "Pawns should capture enemy pieces diagonally.");
        expect(!containsCell(validMoves, {7, 7}),
            "Pawns should not gain diagonal non-capturing moves.");
    }

    void testPawnThreatSquaresStayDiagonal() {
        GameConfig config;
        Board board;
        board.init(8);

        Kingdom white(KingdomId::White);
        Piece& pawn = addPieceToBoard(white, board, 710, PieceType::Pawn, KingdomId::White, {8, 8});

        const std::vector<sf::Vector2i> threatenedSquares = MovementRules::getThreatenedSquares(pawn, board, config);
        expect(containsCell(threatenedSquares, {7, 7})
            && containsCell(threatenedSquares, {9, 7})
            && containsCell(threatenedSquares, {7, 9})
            && containsCell(threatenedSquares, {9, 9}),
            "Pawn threat squares should be the four diagonal adjacent cells.");
        expect(!containsCell(threatenedSquares, {8, 7}) && !containsCell(threatenedSquares, {8, 9}),
            "Pawn threat squares should no longer include orthogonal cells.");
    }

    void testCheckSystemUsesPawnDiagonalThreats() {
        GameConfig config;
        Board board;
        board.init(8);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        addPieceToBoard(white, board, 715, PieceType::King, KingdomId::White, {9, 9});
        addPieceToBoard(black, board, 716, PieceType::Pawn, KingdomId::Black, {8, 8});

        expect(CheckSystem::isInCheck(KingdomId::White, board, config),
            "Runtime check detection should treat diagonal pawn attacks as checks.");
        expect(!CheckSystem::isInCheck(KingdomId::Black, board, config),
            "A pawn should not create a false orthogonal check against its own kingdom setup.");
    }

    void testPawnCapturesEnemyStructuresDiagonallyOnly() {
        GameConfig config;
        Board board;
        board.init(8);

        Kingdom white(KingdomId::White);
        Piece& pawn = addPieceToBoard(white, board, 720, PieceType::Pawn, KingdomId::White, {8, 8});

        Building diagonalWall = makeTestStoneWall(800, KingdomId::Black, {9, 9}, config);
        Building orthogonalWall = makeTestStoneWall(801, KingdomId::Black, {8, 9}, config);
        linkBuildingOnBoard(diagonalWall, board);
        linkBuildingOnBoard(orthogonalWall, board);

        const std::vector<sf::Vector2i> validMoves = MovementRules::getValidMoves(pawn, board, config);
        expect(containsCell(validMoves, {9, 9}),
            "Pawns should attack enemy structures diagonally.");
        expect(!containsCell(validMoves, {8, 9}),
            "Pawns should not attack enemy structures orthogonally anymore.");
    }

    void testForwardModelPawnRulesMatchRuntimeSemantics() {
        GameConfig config;
        Board board;
        board.init(8);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        addPieceToBoard(white, board, 730, PieceType::Pawn, KingdomId::White, {8, 8});
        addPieceToBoard(black, board, 731, PieceType::Knight, KingdomId::Black, {9, 9});
        addPieceToBoard(black, board, 732, PieceType::Bishop, KingdomId::Black, {8, 9});

        black.addBuilding(makeTestStoneWall(810, KingdomId::Black, {7, 9}, config));
        linkBuildingOnBoard(black.buildings.back(), board);

        GameSnapshot snapshot = ForwardModel::createSnapshot(board, white, black, {}, 1);
        const SnapPiece* snapPawn = snapshot.white.getPieceById(730);
        expect(snapPawn != nullptr,
            "The snapshot pawn test setup should preserve the white pawn.");

        const std::vector<sf::Vector2i> pseudoLegalMoves = ForwardModel::getPseudoLegalMoves(
            snapshot, *snapPawn, config.getGlobalMaxRange());
        expect(containsCell(pseudoLegalMoves, {8, 7}),
            "ForwardModel pawns should keep orthogonal non-capturing moves.");
        expect(containsCell(pseudoLegalMoves, {9, 9}),
            "ForwardModel pawns should capture enemy pieces diagonally.");
        expect(containsCell(pseudoLegalMoves, {7, 9}),
            "ForwardModel pawns should capture enemy structures diagonally.");
        expect(!containsCell(pseudoLegalMoves, {8, 9}),
            "ForwardModel pawns should not treat orthogonal enemy contact as a capture.");

        const ThreatMap threats = ForwardModel::buildThreatMap(snapshot, KingdomId::White, config.getGlobalMaxRange());
        expect(threats.isSet({7, 7}) && threats.isSet({9, 9}),
            "ForwardModel pawn threat maps should include diagonal attack squares.");
        expect(!threats.isSet({8, 7}) && !threats.isSet({8, 9}),
            "ForwardModel pawn threat maps should exclude orthogonal squares.");
    }

        void testCheckResponseAllowsKingSidestepAgainstRookCheck() {
         GameConfig config;
         Board board;
         board.init(12);
         Kingdom white(KingdomId::White);
         Kingdom black(KingdomId::Black);

         addPieceToBoard(white, board, 131, PieceType::King, KingdomId::White, {4, 4});
         addPieceToBoard(white, board, 132, PieceType::Rook, KingdomId::White, {10, 12});
         Piece& blackKing = addPieceToBoard(black, board, 231, PieceType::King, KingdomId::Black, {10, 10});

         const std::vector<sf::Vector2i> legalMoves = CheckResponseRules::filterLegalMovesForPiece(
             blackKing, board, config);
         expect(std::find(legalMoves.begin(), legalMoves.end(), sf::Vector2i{9, 10}) != legalMoves.end(),
             "A checked king should keep a legal sidestep escape when one exists.");

         const std::vector<Building> publicBuildings;
         const CheckTurnValidation validation = CheckResponseRules::validatePendingTurn(
             black, white, board, publicBuildings, 1, {}, config);
         expect(validation.activeKingInCheck,
             "The king should be recognized as checked in the sidestep regression setup.");
         expect(validation.hasAnyLegalResponse,
             "The validator must recognize that the checked king can still sidestep out of check.");
         expect(snapshotHasAnyLegalResponse(board, black, white, publicBuildings, 1, config),
             "Snapshot evaluation should also find the sidestep response in the same position.");
        }

        void testCheckResponseAllowsEdgeKingEscapeFromRookCheck() {
         GameConfig config;
         Board board;
         board.init(25);
         Kingdom white(KingdomId::White);
         Kingdom black(KingdomId::Black);

         addPieceToBoard(white, board, 140, PieceType::King, KingdomId::White, {32, 20});
         addPieceToBoard(white, board, 141, PieceType::Rook, KingdomId::White, {24, 24});
         addPieceToBoard(white, board, 142, PieceType::Rook, KingdomId::White, {47, 16});
         Piece& blackKing = addPieceToBoard(black, board, 240, PieceType::King, KingdomId::Black, {47, 14});

         const std::vector<sf::Vector2i> legalMoves = CheckResponseRules::filterLegalMovesForPiece(
             blackKing, board, config);
         expect(std::find(legalMoves.begin(), legalMoves.end(), sf::Vector2i{46, 14}) != legalMoves.end(),
             "The edge king should be able to escape left from the reported false-checkmate position.");

         const std::vector<Building> publicBuildings;
         const CheckTurnValidation validation = CheckResponseRules::validatePendingTurn(
             black, white, board, publicBuildings, 125, {}, config);
         expect(validation.activeKingInCheck,
             "The edge regression setup should still be a real check.");
         expect(validation.hasAnyLegalResponse,
             "The validator must not classify the reported edge position as checkmate.");
         expect(snapshotHasAnyLegalResponse(board, black, white, publicBuildings, 125, config),
             "Snapshot evaluation should confirm the king escape from the edge regression setup.");
        }

        void testCheckResponseDetectsTrueEdgeCheckmate() {
         GameConfig config;
         Board board;
         board.init(25);
         Kingdom white(KingdomId::White);
         Kingdom black(KingdomId::Black);

         addPieceToBoard(white, board, 150, PieceType::King, KingdomId::White, {45, 13});
         addPieceToBoard(white, board, 151, PieceType::Rook, KingdomId::White, {47, 16});
         addPieceToBoard(white, board, 152, PieceType::Rook, KingdomId::White, {45, 15});
         Piece& blackKing = addPieceToBoard(black, board, 250, PieceType::King, KingdomId::Black, {47, 14});

         const std::vector<sf::Vector2i> legalMoves = CheckResponseRules::filterLegalMovesForPiece(
             blackKing, board, config);
         expect(legalMoves.empty(),
             "The edge control setup should leave the checked king with no legal move.");

         const std::vector<Building> publicBuildings;
         const CheckTurnValidation validation = CheckResponseRules::validatePendingTurn(
             black, white, board, publicBuildings, 1, {}, config);
         expect(validation.activeKingInCheck,
             "The edge control setup must still be recognized as check.");
         expect(!validation.hasAnyLegalResponse,
             "The validator should report no legal response in a true edge checkmate.");
         expect(!snapshotHasAnyLegalResponse(board, black, white, publicBuildings, 1, config),
             "Snapshot evaluation should agree that the edge control setup is a real checkmate.");
        }

        void testInteractionPermissionsAllowReadOnlyInspectionOutsideTurn() {
         InteractionPermissionInputs inputs;
         inputs.gameState = GameState::Playing;
         inputs.multiplayerSessionReady = true;
         inputs.isLocalPlayerTurn = false;

         const InteractionPermissions permissions = computeInteractionPermissions(inputs);
         expect(permissions.canInspectWorld,
             "Read-only world inspection should stay available when it is not the local player's turn.");
         expect(permissions.canUseToolbar,
             "Toolbar access should stay available when the player can only inspect the world.");
         expect(permissions.canOpenBuildPanel,
             "The build panel should remain openable even when actual commands are blocked.");
         expect(!permissions.canIssueCommands,
             "Players must not issue commands outside their actionable turn.");
         expect(!permissions.canShowActionOverlays,
             "Read-only inspection states must not render actionable overlays.");
         expect(permissions.canShowBuildPreview,
             "Read-only inspection states should keep the build ghost visible so placement feedback can update without new mouse movement.");
        }

        void testInteractionPermissionsKeepBuildPanelReadOnlyDuringCheck() {
         InteractionPermissionInputs inputs;
         inputs.gameState = GameState::Playing;
         inputs.multiplayerSessionReady = true;
         inputs.isLocalPlayerTurn = true;
         inputs.activeKingInCheck = true;
         inputs.projectedKingInCheck = true;
         inputs.hasAnyLegalResponse = true;

         const InteractionPermissions permissions = computeInteractionPermissions(inputs);
         expect(permissions.canIssueCommands,
             "The active kingdom should still be able to issue legal response moves while in check.");
         expect(!permissions.canQueueNonMoveActions,
             "Non-move actions must stay blocked while the active king is in check.");
         expect(permissions.canOpenBuildPanel,
             "The build panel should remain openable while in check so the UI can show disabled actions.");
        }

        void testInteractionPermissionsUnlockNonMoveActionsAfterQueuedCheckResponse() {
         InteractionPermissionInputs inputs;
         inputs.gameState = GameState::Playing;
         inputs.multiplayerSessionReady = true;
         inputs.isLocalPlayerTurn = true;
         inputs.activeKingInCheck = true;
         inputs.projectedKingInCheck = false;
         inputs.hasAnyLegalResponse = true;

         const InteractionPermissions permissions = computeInteractionPermissions(inputs);
         expect(permissions.canIssueCommands,
             "The active kingdom should still be actionable while it is assembling a legal response to check.");
         expect(permissions.canQueueNonMoveActions,
             "Non-move actions should reopen once the queued move sequence has already resolved the check in projection.");
        }

        void testInteractionPermissionsKeepNavigationAvailableDuringGameOver() {
         InteractionPermissionInputs inputs;
         inputs.gameState = GameState::GameOver;

         const InteractionPermissions permissions = computeInteractionPermissions(inputs);
         expect(permissions.canOpenMenu,
             "Game-over state should still allow opening the in-game menu.");
         expect(permissions.canMoveCamera,
             "Game-over state should still allow camera movement.");
         expect(permissions.canInspectWorld,
             "Game-over state should still allow selecting and inspecting the world.");
         expect(permissions.canUseToolbar,
             "Game-over state should still allow switching between inspection tools.");
         expect(permissions.canOpenBuildPanel,
             "Game-over state should still allow opening the read-only build panel.");
         expect(!permissions.canIssueCommands,
             "Game-over state must block gameplay commands.");
         expect(permissions.canShowBuildPreview,
             "Game-over state should keep the build preview visible for read-only inspection.");
        }

        void testTurnSystemMoveLogIncludesPieceType() {
         GameConfig config;
         Board board;
         board.init(8);

         Kingdom white(KingdomId::White);
         Kingdom black(KingdomId::Black);
         addPieceToBoard(white, board, 150, PieceType::King, KingdomId::White, {2, 2});
         Piece& bishop = addPieceToBoard(white, board, 151, PieceType::Bishop, KingdomId::White, {4, 4});
         addPieceToBoard(black, board, 250, PieceType::King, KingdomId::Black, {12, 12});

         std::vector<Building> publicBuildings;
         TurnSystem turnSystem;
         turnSystem.setActiveKingdom(KingdomId::White);

         TurnCommand moveCommand;
         moveCommand.type = TurnCommand::Move;
         moveCommand.pieceId = bishop.id;
         moveCommand.origin = bishop.position;
         moveCommand.destination = {6, 6};

         expect(turnSystem.queueCommand(moveCommand, board, white, black, publicBuildings, config),
             "Bishop move command should be accepted for move log regression coverage.");

         EventLog eventLog;
         PieceFactory pieceFactory;
         BuildingFactory buildingFactory;
         turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

         const auto& events = eventLog.getEvents();
         const bool foundMoveEvent = std::any_of(events.begin(), events.end(), [](const EventLog::Event& event) {
             return event.message == "Moved Bishop to (6,6)";
         });
         expect(foundMoveEvent,
             "Move history entries should include the moved piece type.");
        }

    void testInGameViewModelShowsCheckAlertWhenActiveKingIsInCheck() {
        GameConfig config;
        GameEngine engine;
        engine.board().init(8);
        engine.kingdom(KingdomId::White) = Kingdom(KingdomId::White);
        engine.kingdom(KingdomId::Black) = Kingdom(KingdomId::Black);
        engine.turnSystem().setActiveKingdom(KingdomId::White);

        addPieceToBoard(engine.kingdom(KingdomId::White), engine.board(), 140, PieceType::King, KingdomId::White, {8, 8});
        addPieceToBoard(engine.kingdom(KingdomId::Black), engine.board(), 240, PieceType::Rook, KingdomId::Black, {8, 4});

        const InGameViewModel model = buildInGameViewModel(
            engine,
            config,
            GameState::Playing,
            true,
            makeHudPresentation(KingdomId::White));
         expect(model.alerts.size() == 1,
             "The dashboard model should expose a single alert when the active king is in check.");
         expect(model.alerts.front().text == "Check",
             "The dashboard model should expose a Check alert when the active king is in check.");
         expect(model.alerts.front().tone == InGameAlertTone::Danger,
             "The Check alert should use the danger tone.");
    }

    void testInGameViewModelUsesPresentedHudKingdom() {
        GameConfig config;
        GameEngine engine;
        engine.board().init(10);
        engine.kingdom(KingdomId::White) = Kingdom(KingdomId::White);
        engine.kingdom(KingdomId::Black) = Kingdom(KingdomId::Black);
        engine.turnSystem().setActiveKingdom(KingdomId::White);
        engine.turnSystem().setTurnNumber(3);

        Kingdom& white = engine.kingdom(KingdomId::White);
        Kingdom& black = engine.kingdom(KingdomId::Black);
        white.gold = 12;
        black.gold = 73;

        addPieceToBoard(white, engine.board(), 150, PieceType::King, KingdomId::White, {8, 8});
        addPieceToBoard(black, engine.board(), 250, PieceType::King, KingdomId::Black, {2, 2});
        addPieceToBoard(black, engine.board(), 251, PieceType::Rook, KingdomId::Black, {3, 2});
        black.buildings.push_back(makeTestBarracks(44, KingdomId::Black, {4, 4}, config));
        linkBuildingOnBoard(black.buildings.back(), engine.board());

        const InGameViewModel model = buildInGameViewModel(
            engine,
            config,
            GameState::Playing,
            false,
            makeHudPresentation(KingdomId::Black,
                               false,
                               InGameTurnIndicatorTone::LocalTurn));
        expect(model.activeGold == 73,
            "The dashboard model should expose the gold of the kingdom selected for HUD stats.");
        expect(model.activeTroops == black.pieceCount(),
            "The dashboard model should expose troop counts from the kingdom selected for HUD stats.");
        expect(model.activeOccupiedCells == countOccupiedBuildingCells(black),
            "The dashboard model should expose occupied cells from the kingdom selected for HUD stats.");
        expect(!model.showTurnPointIndicators,
            "The dashboard model should carry whether turn-point indicators must be hidden.");
        expect(model.turnIndicatorTone == InGameTurnIndicatorTone::LocalTurn,
            "The dashboard model should carry the requested turn-indicator tone.");
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

    void testSingleLocalHudModes() {
        GameSessionConfig hotseat = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "hud_hotseat_session");
        expect(!hasSingleLocallyControlledKingdom(makeLocalPlayerContextForSession(hotseat)),
            "Hotseat sessions should keep alternating HUD stats because both kingdoms are local.");

        GameSessionConfig humanVsAI = makeDefaultGameSessionConfig(GameMode::HumanVsAI, "hud_hvai_session");
        expect(hasSingleLocallyControlledKingdom(makeLocalPlayerContextForSession(humanVsAI)),
            "Human vs AI sessions should pin HUD stats to the single local human kingdom.");

        GameSessionConfig aiVsAI = makeDefaultGameSessionConfig(GameMode::AIvsAI, "hud_aivai_session");
        expect(!hasSingleLocallyControlledKingdom(makeLocalPlayerContextForSession(aiVsAI)),
            "AI vs AI sessions should keep alternating HUD stats because there is no local human kingdom.");

        GameSessionConfig host = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "hud_host_session");
        host.multiplayer.enabled = true;
        host.multiplayer.port = 42000;
        host.multiplayer.passwordHash = "hash";
        host.multiplayer.passwordSalt = "salt";
        expect(hasSingleLocallyControlledKingdom(makeLocalPlayerContextForSession(host)),
            "LAN host sessions should use the single-local HUD rules.");

        expect(hasSingleLocallyControlledKingdom(makeLanClientLocalPlayerContext()),
            "LAN client sessions should use the single-local HUD rules.");
    }

    void testMultiplayerRuntimeReconnectStateLifecycle() {
        MultiplayerRuntime runtime;
        const MultiplayerJoinCredentials credentials{"127.0.0.1", 4242, "secret"};

        runtime.cacheReconnectRequest(credentials);
        expect(runtime.hasReconnectRequest(),
            "Caching reconnect credentials should make the reconnect request available.");
        expect(runtime.reconnectRequest().host == "127.0.0.1"
         && runtime.reconnectRequest().port == 4242
         && runtime.reconnectRequest().password == "secret",
            "Cached reconnect credentials should round-trip through the runtime.");
        expect(!runtime.awaitingReconnect(),
            "Caching reconnect credentials should not mark the runtime as awaiting reconnect.");

        runtime.noteReconnectAwaiting("Lost host connection.");
        expect(runtime.awaitingReconnect(),
            "Disconnect handling should mark the runtime as awaiting reconnect.");
        expect(runtime.reconnectLastErrorMessage() == "Lost host connection.",
            "The reconnect state should preserve the last disconnect reason.");

        runtime.noteReconnectRecovered();
        expect(!runtime.awaitingReconnect(),
            "A recovered reconnect should clear the awaiting-reconnect flag.");
        expect(runtime.reconnectLastErrorMessage().empty(),
            "A recovered reconnect should clear the last disconnect reason.");

        runtime.clearReconnectState();
        expect(!runtime.hasReconnectRequest(),
            "Clearing reconnect state should forget the cached reconnect request.");
    }

    void testSessionFlowStartsSavesAndLoadsSession() {
        GameConfig config;
        GameEngine engine;
        SaveManager saveManager;
        MultiplayerRuntime multiplayer;
        GameStateDebugRecorder debugRecorder;
        const auto tempDir = std::filesystem::temp_directory_path()
            / ("anormalchessgame_sessionflow_"
                + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(tempDir);

        try {
            const std::string saveName = "session_flow_roundtrip";
            SessionFlow flow(engine,
                             saveManager,
                             multiplayer,
                             debugRecorder,
                             config,
                             tempDir.string());
            GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, saveName);

            std::string error;
            expect(flow.startNewSession(session, &error), error);
            expect(flow.saveAuthoritativeSession(true, &error), error);
            expect(std::filesystem::exists(tempDir / (saveName + ".json")),
                   "SessionFlow should write the authoritative save file into the configured directory.");

            GameEngine restoredEngine;
            SaveManager restoredSaveManager;
            MultiplayerRuntime restoredMultiplayer;
            GameStateDebugRecorder restoredDebugRecorder;
            SessionFlow restoredFlow(restoredEngine,
                                     restoredSaveManager,
                                     restoredMultiplayer,
                                     restoredDebugRecorder,
                                     config,
                                     tempDir.string());

            expect(restoredFlow.loadSession(saveName, &error), error);
            expect(restoredEngine.gameName() == saveName,
                   "Loading through SessionFlow should restore the saved session name.");
            expect(restoredEngine.turnSystem().getTurnNumber() == 1,
                   "Loading a freshly saved session through SessionFlow should restore the starting turn number.");
            expect(restoredEngine.validate(&error), error);
        } catch (...) {
            std::filesystem::remove_all(tempDir);
            throw;
        }

        std::filesystem::remove_all(tempDir);
    }

    void testFrontendCoordinatorBuildsHudAndLocksWaitingTurns() {
        FrontendRuntimeState state;
        state.gameState = GameState::Playing;
        state.localPlayerContext = makeLanClientLocalPlayerContext();
        state.activeKingdom = KingdomId::Black;
        state.clientAuthenticated = true;

        const InGameHudPresentation hud = FrontendCoordinator::buildInGameHudPresentation(state);
        expect(hud.statsKingdom == KingdomId::Black,
            "FrontendCoordinator should pin HUD stats to the local LAN client kingdom on the local turn.");
        expect(hud.showTurnPointIndicators,
            "FrontendCoordinator should keep turn-point indicators visible on the local turn.");
        expect(hud.turnIndicatorTone == InGameTurnIndicatorTone::LocalTurn,
            "FrontendCoordinator should highlight the local turn when the local LAN client can act.");

        const InteractionPermissions activePermissions =
            FrontendCoordinator::currentInteractionPermissions(state, std::nullopt);
        expect(activePermissions.canIssueCommands,
            "FrontendCoordinator should allow commands when the local LAN client is authenticated and not waiting.");
        expect(activePermissions.canQueueNonMoveActions,
            "FrontendCoordinator should allow non-move actions when no projected check restriction is present.");

        state.waitingForRemoteTurnResult = true;
        const InteractionPermissions waitingPermissions =
            FrontendCoordinator::currentInteractionPermissions(state, std::nullopt);
        expect(!waitingPermissions.canIssueCommands,
            "FrontendCoordinator should lock commands while waiting for the host to confirm the submitted turn.");
        expect(!waitingPermissions.canQueueNonMoveActions,
            "FrontendCoordinator should also lock queued non-move actions while waiting for host confirmation.");
        expect(waitingPermissions.canInspectWorld,
            "FrontendCoordinator should still allow world inspection while waiting for remote turn confirmation.");
    }

    void testInteractivePermissionsCacheReusesMatchingRuntimeState() {
        InteractivePermissionsCache cache;
        FrontendRuntimeState state;
        state.gameState = GameState::Playing;
        state.localPlayerContext = makeLanClientLocalPlayerContext();
        state.activeKingdom = KingdomId::Black;
        state.clientAuthenticated = true;

        int resolveCount = 0;
        const InteractionPermissions first = cache.resolve(state, [&]() {
            ++resolveCount;
            InteractionPermissions permissions;
            permissions.canMoveCamera = true;
            permissions.canIssueCommands = true;
            return permissions;
        });
        const InteractionPermissions second = cache.resolve(state, [&]() {
            ++resolveCount;
            InteractionPermissions permissions;
            permissions.canMoveCamera = false;
            return permissions;
        });

        expect(resolveCount == 1,
            "InteractivePermissionsCache should reuse cached permissions while the frontend runtime state is unchanged.");
        expect(first.canMoveCamera && second.canMoveCamera,
            "InteractivePermissionsCache should keep returning the cached permissions until the runtime state changes.");
    }

    void testInteractivePermissionsCacheInvalidatesOnStateChangeAndManualReset() {
        InteractivePermissionsCache cache;
        FrontendRuntimeState state;
        state.gameState = GameState::Playing;
        state.localPlayerContext = makeLanClientLocalPlayerContext();
        state.activeKingdom = KingdomId::Black;
        state.clientAuthenticated = true;

        int resolveCount = 0;
        const InteractionPermissions initial = cache.resolve(state, [&]() {
            ++resolveCount;
            InteractionPermissions permissions;
            permissions.canIssueCommands = true;
            return permissions;
        });

        FrontendRuntimeState waitingState = state;
        waitingState.waitingForRemoteTurnResult = true;
        const InteractionPermissions afterStateChange = cache.resolve(waitingState, [&]() {
            ++resolveCount;
            InteractionPermissions permissions;
            permissions.canIssueCommands = false;
            return permissions;
        });

        cache.invalidate();
        const InteractionPermissions afterManualInvalidation = cache.resolve(waitingState, [&]() {
            ++resolveCount;
            InteractionPermissions permissions;
            permissions.canIssueCommands = true;
            return permissions;
        });

        expect(resolveCount == 3,
            "InteractivePermissionsCache should recompute after runtime-state changes and after explicit invalidation.");
        expect(initial.canIssueCommands,
            "InteractivePermissionsCache should expose the initial resolver result before any invalidation occurs.");
        expect(!afterStateChange.canIssueCommands,
            "InteractivePermissionsCache should refresh when the runtime state changes.");
        expect(afterManualInvalidation.canIssueCommands,
            "InteractivePermissionsCache should refresh after manual invalidation even if the runtime state stays the same.");
    }

    void testPendingTurnValidationCacheReusesMatchingTurnState() {
        PendingTurnValidationCache cache;
        const PendingTurnValidationCacheKey key{17u, KingdomId::White, 9};

        int resolveCount = 0;
        const CheckTurnValidation& first = cache.resolve(key, [&]() {
            ++resolveCount;
            CheckTurnValidation validation;
            validation.valid = false;
            validation.bankrupt = true;
            validation.projectedEndingGold = -4;
            return validation;
        });
        const CheckTurnValidation& second = cache.resolve(key, [&]() {
            ++resolveCount;
            CheckTurnValidation validation;
            validation.valid = true;
            validation.projectedEndingGold = 12;
            return validation;
        });

        expect(resolveCount == 1,
            "PendingTurnValidationCache should reuse the cached validation while the pending-turn state key is unchanged.");
        expect(!first.valid && second.bankrupt && second.projectedEndingGold == -4,
            "PendingTurnValidationCache should keep returning the cached validation result until the turn-state key changes.");
    }

    void testPendingTurnValidationCacheInvalidatesOnKeyChangesAndManualReset() {
        PendingTurnValidationCache cache;

        int resolveCount = 0;
        const CheckTurnValidation& initial = cache.resolve({3u, KingdomId::White, 11}, [&]() {
            ++resolveCount;
            CheckTurnValidation validation;
            validation.valid = true;
            validation.projectedEndingGold = 8;
            return validation;
        });
        const CheckTurnValidation& afterRevisionChange = cache.resolve({4u, KingdomId::White, 11}, [&]() {
            ++resolveCount;
            CheckTurnValidation validation;
            validation.valid = false;
            validation.activeKingInCheck = true;
            return validation;
        });
        const CheckTurnValidation& afterKingdomChange = cache.resolve({4u, KingdomId::Black, 11}, [&]() {
            ++resolveCount;
            CheckTurnValidation validation;
            validation.hasAnyLegalResponse = true;
            return validation;
        });
        const CheckTurnValidation& afterTurnChange = cache.resolve({4u, KingdomId::Black, 12}, [&]() {
            ++resolveCount;
            CheckTurnValidation validation;
            validation.valid = false;
            validation.projectedKingInCheck = true;
            return validation;
        });

        cache.invalidate();
        const CheckTurnValidation& afterManualInvalidation = cache.resolve({4u, KingdomId::Black, 12}, [&]() {
            ++resolveCount;
            CheckTurnValidation validation;
            validation.bankrupt = true;
            validation.projectedEndingGold = -9;
            return validation;
        });

        expect(resolveCount == 5,
            "PendingTurnValidationCache should recompute when the revision, active kingdom, turn number, or manual invalidation changes the cache state.");
        expect(initial.valid && initial.projectedEndingGold == 8,
            "PendingTurnValidationCache should expose the initial resolver result before later invalidations occur.");
        expect(!afterRevisionChange.valid && afterRevisionChange.activeKingInCheck,
            "PendingTurnValidationCache should refresh when the pending-state revision changes.");
        expect(afterKingdomChange.hasAnyLegalResponse,
            "PendingTurnValidationCache should refresh when the active kingdom changes.");
        expect(!afterTurnChange.valid && afterTurnChange.projectedKingInCheck,
            "PendingTurnValidationCache should refresh when the turn number changes.");
        expect(afterManualInvalidation.bankrupt && afterManualInvalidation.projectedEndingGold == -9,
            "PendingTurnValidationCache should refresh after manual invalidation even if the key stays the same.");
    }

    void testFrontendCoordinatorBuildsProjectedDashboardAndPiecePanel() {
        GameConfig config;
        GameEngine engine;
        GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman,
                                                                 "frontend_dashboard_projection_test");

        std::string error;
        expect(engine.startNewSession(session, config, &error), error);

        FrontendRuntimeState state;
        state.gameState = GameState::Playing;
        state.localPlayerContext = makeLocalPlayerContextForSession(session);
        state.activeKingdom = engine.turnSystem().getActiveKingdom();

        const InteractionPermissions permissions =
            FrontendCoordinator::currentInteractionPermissions(state, std::nullopt);
        const InGameHudPresentation hud = FrontendCoordinator::buildInGameHudPresentation(state);

        std::array<Kingdom, kNumKingdoms> projectedKingdoms = engine.kingdoms();
        projectedKingdoms[kingdomIndex(KingdomId::White)].gold += 37;
        projectedKingdoms[kingdomIndex(KingdomId::White)].pieces.push_back(
            Piece(9999, PieceType::Pawn, KingdomId::White, {0, 0}));

        CheckTurnValidation validation;
        validation.valid = false;
        validation.bankrupt = true;
        validation.projectedEndingGold = -5;

        const FrontendDashboardBindings dashboardBindings{
            engine,
            engine.board(),
            projectedKingdoms,
            engine.publicBuildings(),
            config
        };
        const InGameViewModel projectedViewModel = FrontendCoordinator::buildDashboardViewModel(
            state,
            dashboardBindings,
            permissions,
            validation,
            hud,
            true);

        expect(projectedViewModel.activeGold == projectedKingdoms[kingdomIndex(KingdomId::White)].gold,
            "FrontendCoordinator should surface projected gold from the displayed kingdom state.");
        expect(projectedViewModel.activeTroops == projectedKingdoms[kingdomIndex(KingdomId::White)].pieceCount(),
            "FrontendCoordinator should surface projected troop counts from the displayed kingdom state.");
        expect(projectedViewModel.balanceMetrics[0].whiteValue
                == projectedKingdoms[kingdomIndex(KingdomId::White)].gold,
            "FrontendCoordinator should project white balance metrics from the displayed kingdom state.");
        expect(!projectedViewModel.canEndTurn,
            "FrontendCoordinator should keep end-turn disabled when pending validation is invalid.");
        expect(!projectedViewModel.alerts.empty()
                && projectedViewModel.alerts.front().text.find("Bankruptcy") != std::string::npos,
            "FrontendCoordinator should surface pending bankruptcy warnings in the dashboard model.");

        const sf::Vector2i extraPieceCell = findEmptyTraversableCell(engine);
        Piece& selectedPiece = addPieceToBoard(engine.kingdom(KingdomId::White),
                                               engine.board(),
                                               9998,
                                               PieceType::Pawn,
                                               KingdomId::White,
                                               extraPieceCell);

        const FrontendPanelBindings panelBindings{
            ToolState::Select,
            engine.board(),
            engine.turnSystem(),
            config,
            &selectedPiece,
            nullptr,
            nullptr
        };
        const FrontendLeftPanelPresentation panelPresentation =
            FrontendCoordinator::buildLeftPanelPresentation(state, panelBindings, permissions);

        expect(panelPresentation.kind == FrontendLeftPanelKind::Piece,
            "FrontendCoordinator should route a selected piece to the piece panel presentation.");
        expect(panelPresentation.piece == &selectedPiece,
            "FrontendCoordinator should preserve the selected piece pointer in the panel presentation.");
        expect(panelPresentation.allowUpgrade,
            "FrontendCoordinator should allow upgrades on a locally controlled selected piece when actions are unlocked.");
        expect(panelPresentation.allowDisband,
            "FrontendCoordinator should allow disband on a locally controlled non-king piece when no conflicting action is queued.");
    }

    void testMultiplayerEventCoordinatorPlansAlertsAndDisconnects() {
        MultiplayerServer::Event hostEvent;
        hostEvent.type = MultiplayerServer::Event::Type::ClientDisconnected;
        hostEvent.message = "Black disconnected from the host.";

        const MultiplayerServerEventPlan hostPlan = MultiplayerEventCoordinator::planServerEvent(
            hostEvent,
            MultiplayerHostEventState{8, true});
        expect(hostPlan.remoteSessionEstablished.has_value() && !*hostPlan.remoteSessionEstablished,
            "MultiplayerEventCoordinator should clear the host remote-session-established flag after a disconnect.");
        expect(hostPlan.logMessage.has_value() && *hostPlan.logMessage == hostEvent.message,
            "MultiplayerEventCoordinator should preserve the host disconnect log message.");
        expect(hostPlan.alert.has_value() && hostPlan.alert->title == "Black Disconnected",
            "MultiplayerEventCoordinator should raise a blocking host alert after an established Black disconnect.");

        MultiplayerClient::Event clientEvent;
        clientEvent.type = MultiplayerClient::Event::Type::Disconnected;
        const MultiplayerClientEventPlan clientPlan = MultiplayerEventCoordinator::planClientEvent(clientEvent);
        expect(clientPlan.type == MultiplayerClientEventPlan::Type::Disconnect,
            "MultiplayerEventCoordinator should classify disconnected client packets as disconnect events.");
        expect(clientPlan.title == "Host Disconnected",
            "MultiplayerEventCoordinator should expose the host disconnect alert title.");
        expect(clientPlan.message == "Multiplayer host disconnected.",
            "MultiplayerEventCoordinator should provide the default disconnect message when the host transport closes silently.");

        const MultiplayerAlertPlan reconnectAlert = MultiplayerEventCoordinator::buildClientDisconnectAlert(
            MultiplayerReconnectDialogState{true},
            clientPlan.title,
            clientPlan.message);
        expect(reconnectAlert.primaryAction.kind == MultiplayerAlertActionKind::Reconnect,
            "MultiplayerEventCoordinator should prioritize reconnect when a cached host request exists.");
        expect(reconnectAlert.secondaryAction.has_value()
                && reconnectAlert.secondaryAction->kind == MultiplayerAlertActionKind::ReturnToMainMenu,
            "MultiplayerEventCoordinator should keep return-to-menu as the fallback disconnect action.");
    }

    void testMultiplayerEventCoordinatorRestoresSnapshotsAndClientState() {
        GameConfig config;
        GameEngine engine;
        GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman,
                                                                 "multiplayer_event_restore_test");

        std::string error;
        expect(engine.startNewSession(session, config, &error), error);
        expect(engine.triggerCheatcodeWeatherFront(config),
            "Multiplayer snapshot restore test should force a fog front so the host serializes a concrete authoritative weather mask.");
        engine.ensureWeatherMaskUpToDate(config);
        expect(!engine.weatherMaskCache().alphaByCell.empty(),
            "Multiplayer snapshot restore test should build a concrete host weather mask before serializing the snapshot.");

        SaveManager saveManager;
        const std::string serializedSnapshot = saveManager.serialize(engine.createSaveData());

        GameEngine restoredEngine;
        expect(MultiplayerEventCoordinator::restoreClientSnapshot(
                   serializedSnapshot,
                   saveManager,
                   restoredEngine,
                   config,
                   &error),
               error);
        expect(restoredEngine.gameName() == engine.gameName(),
            "MultiplayerEventCoordinator should restore the serialized multiplayer snapshot into the engine.");
        expect(restoredEngine.validate(&error), error);
        expect(restoredEngine.weatherSystemState().revision == engine.weatherSystemState().revision,
            "MultiplayerEventCoordinator should restore the authoritative weather revision from the host snapshot.");
        expect(restoredEngine.weatherMaskCache().revision == engine.weatherMaskCache().revision
                && restoredEngine.weatherMaskCache().diameter == engine.weatherMaskCache().diameter
                && restoredEngine.weatherMaskCache().alphaByCell == engine.weatherMaskCache().alphaByCell
                && restoredEngine.weatherMaskCache().shadeByCell == engine.weatherMaskCache().shadeByCell,
            "MultiplayerEventCoordinator should restore the exact authoritative weather mask from the host snapshot so clients do not locally regenerate divergent fog shapes.");

        MultiplayerRuntime runtime;
        runtime.noteReconnectAwaiting("Connection lost.");
        InputHandler input;
        input.setTool(ToolState::Build);
        LocalPlayerContext localPlayerContext;
        bool waitingForRemoteTurnResult = true;

        MultiplayerEventCoordinator::applyClientSnapshotState(
            runtime,
            input,
            localPlayerContext,
            waitingForRemoteTurnResult);
        expect(localPlayerContext.mode == LocalSessionMode::LanClient,
            "MultiplayerEventCoordinator should restore LAN client local context after a host snapshot arrives.");
        expect(!waitingForRemoteTurnResult,
            "MultiplayerEventCoordinator should clear remote-turn waiting after a fresh host snapshot arrives.");
        expect(!runtime.awaitingReconnect(),
            "MultiplayerEventCoordinator should clear the reconnect-awaiting flag after a successful snapshot restore.");

        input.setTool(ToolState::Build);
        waitingForRemoteTurnResult = true;
        MultiplayerEventCoordinator::applyClientDisconnectState(
            runtime,
            restoredEngine,
            input,
            localPlayerContext,
            waitingForRemoteTurnResult);
        expect(localPlayerContext.mode == LocalSessionMode::LanClient,
            "MultiplayerEventCoordinator should preserve LAN client perspective after a disconnect reset.");
        expect(!waitingForRemoteTurnResult,
            "MultiplayerEventCoordinator should clear remote-turn waiting during disconnect cleanup.");
        expect(input.getCurrentTool() == ToolState::Select,
            "MultiplayerEventCoordinator should force select mode during disconnect cleanup.");
        expect(restoredEngine.turnSystem().getPendingCommands().empty(),
            "MultiplayerEventCoordinator should clear pending commands during disconnect cleanup.");
    }

    void testMultiplayerRuntimeCoordinatorBuildsDialogActions() {
        bool returnedToMainMenu = false;
        bool reconnectAttemptInProgress = false;
        bool reconnectCalled = false;
        std::string shownReconnectFailureTitle;
        std::string shownReconnectFailureMessage;

        MultiplayerAlertActionBindings bindings;
        bindings.onReturnToMainMenu = [&returnedToMainMenu]() {
            returnedToMainMenu = true;
        };
        bindings.onReconnectToMultiplayerHost =
            [&reconnectCalled](std::string* errorMessage) {
                reconnectCalled = true;
                if (errorMessage) {
                    *errorMessage = "Timed out";
                }
                return false;
            };
        bindings.onShowReconnectFailure =
            [&shownReconnectFailureTitle, &shownReconnectFailureMessage](const std::string& title,
                                                                         const std::string& message) {
                shownReconnectFailureTitle = title;
                shownReconnectFailureMessage = message;
            };
        bindings.reconnectAttemptInProgress = [&reconnectAttemptInProgress]() {
            return reconnectAttemptInProgress;
        };
        bindings.setReconnectAttemptInProgress = [&reconnectAttemptInProgress](bool inProgress) {
            reconnectAttemptInProgress = inProgress;
        };

        const MultiplayerDialogAction returnAction = MultiplayerRuntimeCoordinator::buildDialogAction(
            MultiplayerAlertActionPlan{MultiplayerAlertActionKind::ReturnToMainMenu, "Return"},
            bindings);
        returnAction.callback();
        expect(returnedToMainMenu,
            "MultiplayerRuntimeCoordinator should preserve the return-to-menu alert action callback.");

        const MultiplayerDialogAction reconnectAction = MultiplayerRuntimeCoordinator::buildDialogAction(
            MultiplayerAlertActionPlan{MultiplayerAlertActionKind::Reconnect, "Reconnect"},
            bindings);
        reconnectAction.callback();
        expect(reconnectCalled,
            "MultiplayerRuntimeCoordinator should invoke the reconnect callback when the alert requests a reconnect.");
        expect(!reconnectAttemptInProgress,
            "MultiplayerRuntimeCoordinator should clear the reconnect-in-progress flag again when reconnect fails immediately.");
        expect(shownReconnectFailureTitle == "Reconnect Failed",
            "MultiplayerRuntimeCoordinator should surface the reconnect failure alert title after a failed reconnect attempt.");
        expect(shownReconnectFailureMessage == MultiplayerEventCoordinator::reconnectFailureMessage("Timed out"),
            "MultiplayerRuntimeCoordinator should surface the standard reconnect failure message after a failed reconnect attempt.");

        reconnectCalled = false;
        reconnectAttemptInProgress = true;
        reconnectAction.callback();
        expect(!reconnectCalled,
            "MultiplayerRuntimeCoordinator should ignore reconnect alert actions while a reconnect attempt is already in progress.");

        const MultiplayerDialogAction continueAction = MultiplayerRuntimeCoordinator::buildDialogAction(
            MultiplayerAlertActionPlan{MultiplayerAlertActionKind::Continue, "Continue"},
            bindings);
        expect(!continueAction.callback,
            "MultiplayerRuntimeCoordinator should keep continue-only alert actions as passive buttons without side effects.");
    }

    void testInputCoordinatorPlansGameplayShortcuts() {
        InputFrameState state;
        state.gameState = GameState::Playing;
        state.permissions.canIssueCommands = true;
        state.permissions.canUseToolbar = true;
        state.permissions.canInspectWorld = true;

        sf::Event escapeEvent{};
        escapeEvent.type = sf::Event::KeyPressed;
        escapeEvent.key.code = sf::Keyboard::Escape;
        expect(InputCoordinator::planPreGuiAction(escapeEvent, state).kind
                   == InputPreGuiActionKind::ToggleInGameMenu,
               "InputCoordinator should route Escape to the in-game menu in interactive gameplay states.");

        sf::Event spaceEvent{};
        spaceEvent.type = sf::Event::KeyPressed;
        spaceEvent.key.code = sf::Keyboard::Space;
        expect(InputCoordinator::planPreGuiAction(spaceEvent, state).kind
                   == InputPreGuiActionKind::CommitTurn,
               "InputCoordinator should route Space to commit the local turn when commands are allowed.");

        state.inGameMenuOpen = true;
        expect(InputCoordinator::planPreGuiAction(spaceEvent, state).kind
                   == InputPreGuiActionKind::SkipEvent,
               "InputCoordinator should swallow blocked gameplay shortcuts while the in-game menu is open.");

        state.inGameMenuOpen = false;
        state.permissions.canUseToolbar = false;
        sf::Event rightClickEvent{};
        rightClickEvent.type = sf::Event::MouseButtonPressed;
        rightClickEvent.mouseButton.button = sf::Mouse::Right;
        expect(InputCoordinator::planPreGuiAction(rightClickEvent, state).kind
                   == InputPreGuiActionKind::SkipEvent,
               "InputCoordinator should still swallow right-click quick-select when the toolbar is locked.");

        state.permissions.canUseToolbar = true;
        expect(InputCoordinator::planPreGuiAction(rightClickEvent, state).kind
                   == InputPreGuiActionKind::ActivateSelectTool,
               "InputCoordinator should route right click to the select tool when toolbar access is allowed.");

        sf::Event tabEvent{};
        tabEvent.type = sf::Event::KeyPressed;
        tabEvent.key.code = sf::Keyboard::Tab;
        expect(InputCoordinator::planPreGuiAction(tabEvent, state).kind
                   == InputPreGuiActionKind::SkipEvent,
               "InputCoordinator should block GUI navigation keys before they leak into the gameplay loop.");
    }

    void testInputCoordinatorPlansCheatcodeShortcuts() {
        InputFrameState state;
        state.gameState = GameState::Playing;
        state.cheatcodeEnabled = true;
        state.cheatcodeWeatherShortcut = sf::Keyboard::Num1;
        state.cheatcodeChestShortcut = sf::Keyboard::Num2;
        state.cheatcodeInfernalShortcut = sf::Keyboard::Num3;
        state.permissions.canIssueCommands = true;
        state.permissions.canInspectWorld = true;

        sf::Event weatherEvent{};
        weatherEvent.type = sf::Event::KeyPressed;
        weatherEvent.key.code = sf::Keyboard::Num1;
        expect(InputCoordinator::planPreGuiAction(weatherEvent, state).kind
                   == InputPreGuiActionKind::TriggerCheatcodeWeather,
               "InputCoordinator should route the configured weather cheat shortcut when cheatcode mode is enabled.");

        sf::Event chestEvent{};
        chestEvent.type = sf::Event::KeyPressed;
        chestEvent.key.code = sf::Keyboard::Num2;
        expect(InputCoordinator::planPreGuiAction(chestEvent, state).kind
                   == InputPreGuiActionKind::TriggerCheatcodeChest,
               "InputCoordinator should route the configured chest cheat shortcut when cheatcode mode is enabled.");

        sf::Event infernalEvent{};
        infernalEvent.type = sf::Event::KeyPressed;
        infernalEvent.key.code = sf::Keyboard::Num3;
        expect(InputCoordinator::planPreGuiAction(infernalEvent, state).kind
                   == InputPreGuiActionKind::TriggerCheatcodeInfernal,
               "InputCoordinator should route the configured infernal cheat shortcut when cheatcode mode is enabled.");

        state.permissions.canIssueCommands = false;
        expect(InputCoordinator::planPreGuiAction(weatherEvent, state).kind
                   == InputPreGuiActionKind::SkipEvent,
               "InputCoordinator should swallow cheat shortcuts when authoritative commands are locked.");

        state.permissions.canIssueCommands = true;
        state.inGameMenuOpen = true;
        expect(InputCoordinator::planPreGuiAction(chestEvent, state).kind
                   == InputPreGuiActionKind::SkipEvent,
               "InputCoordinator should swallow cheat shortcuts while the in-game menu is open.");
    }

    void testInputCoordinatorRoutesWorldInputAfterGuiFiltering() {
        InputFrameState state;
        state.gameState = GameState::Playing;
        state.permissions.canInspectWorld = true;

        expect(InputCoordinator::planPostGuiAction(state, false, false).kind
                   == InputPostGuiActionKind::DispatchToWorld,
               "InputCoordinator should dispatch events to world input when gameplay inspection is available.");
        expect(InputCoordinator::planPostGuiAction(state, true, false).kind
                   == InputPostGuiActionKind::SkipEvent,
               "InputCoordinator should stop world input after GUI widgets consume the event.");

        state.overlaysVisible = true;
        expect(InputCoordinator::planPostGuiAction(state, false, false).kind
                   == InputPostGuiActionKind::SkipEvent,
               "InputCoordinator should block world input while modal multiplayer overlays are visible.");

        state.overlaysVisible = false;
        state.inGameMenuOpen = true;
        expect(InputCoordinator::planPostGuiAction(state, false, false).kind
                   == InputPostGuiActionKind::SkipEvent,
               "InputCoordinator should block world input while the in-game menu is open.");

        state.inGameMenuOpen = false;
        expect(InputCoordinator::planPostGuiAction(state, false, true).kind
                   == InputPostGuiActionKind::SkipEvent,
               "InputCoordinator should block world input when the cursor is over a UI-owned world-space region.");

        state.gameState = GameState::MainMenu;
        expect(InputCoordinator::planPostGuiAction(state, false, false).kind
                   == InputPostGuiActionKind::SkipEvent,
               "InputCoordinator should never dispatch main-menu events into the world input handler.");
    }

    void testRenderCoordinatorBuildsSelectionAndMoveOverlayPlan() {
        GameConfig config;

        Piece selectedPiece(77, PieceType::King, KingdomId::White, {4, 5});
        WorldRenderState state;
        state.gameState = GameState::Playing;
        state.activeTool = ToolState::Select;
        state.permissions.canShowActionOverlays = true;
        state.activeKingdom = KingdomId::White;
        state.selectedPiece = &selectedPiece;
        state.selectedOriginDangerous = true;
        state.validMoves = {{5, 5}, {6, 5}};
        state.dangerMoves = {{4, 6}};
        state.capturePreviewPieceIds.insert(999);

        TurnCommand moveCommand = makeMoveCommand(selectedPiece.id, selectedPiece.position, {5, 5});
        const WorldRenderPlan plan = RenderCoordinator::buildWorldRenderPlan(
            state,
            std::vector<TurnCommand>{moveCommand},
            {},
            {},
            config);

        expect(plan.renderWorld,
            "RenderCoordinator should render the world during interactive in-game states.");
        expect(plan.showOrientationCheckerboard,
            "RenderCoordinator should request the orientation checkerboard when a piece is selected in select mode.");
        expect(plan.capturePreviewPieceIds.count(999) == 1,
            "RenderCoordinator should preserve the capture-preview skip-piece set for the renderer.");
        expect(plan.selectedOriginCell.has_value()
                && plan.selectedOriginCell->origin == selectedPiece.position,
            "RenderCoordinator should highlight the selected origin cell for kings and queued moves.");
        expect(plan.selectedOriginCell.has_value()
                && plan.selectedOriginCell->color == sf::Color(255, 40, 40, 90),
            "RenderCoordinator should color dangerous origin cells in red.");
        expect(plan.highlightedCells.size() == 2,
            "RenderCoordinator should expose reachable move cells for the selected local piece.");
        expect(plan.dangerCells.size() == 1,
            "RenderCoordinator should expose dangerous move cells separately from standard reachable cells.");
        expect(plan.selectionFrames.size() == 1
                && plan.selectionFrames.front().origin == selectedPiece.position,
            "RenderCoordinator should add a selection frame for the selected piece.");
        expect(plan.actionMarkers.size() == 1
                && plan.actionMarkers.front().iconName == "move_ongoing",
            "RenderCoordinator should add move action markers for queued movement commands.");
    }

    void testRenderCoordinatorPrefersAnchoredSelectionCellForPieceFrame() {
        GameConfig config;

        Piece selectedPiece(88, PieceType::King, KingdomId::White, {4, 5});
        WorldRenderState state;
        state.gameState = GameState::Playing;
        state.activeTool = ToolState::Select;
        state.permissions.canShowActionOverlays = true;
        state.activeKingdom = KingdomId::White;
        state.selectedPiece = &selectedPiece;
        state.selectedCell = sf::Vector2i{7, 8};

        const WorldRenderPlan plan = RenderCoordinator::buildWorldRenderPlan(
            state,
            {},
            {},
            {},
            config);

        expect(plan.selectionFrames.size() == 1
                && plan.selectionFrames.front().origin == sf::Vector2i(7, 8),
            "RenderCoordinator should keep the selection frame anchored to the canonical selected cell even when the piece object currently reports a different position.");
        expect(plan.selectedOriginCell.has_value() && plan.selectedOriginCell->origin == sf::Vector2i(7, 8),
            "RenderCoordinator should align the selected origin overlay with the anchored selection cell when no queued move overrides the origin.");
    }

    void testRenderCoordinatorBuildsBuildPreviewAndPendingBuildPlan() {
        GameConfig config;

        WorldRenderState state;
        state.gameState = GameState::Playing;
        state.activeTool = ToolState::Build;
        state.permissions.canShowActionOverlays = true;
        state.permissions.canShowBuildPreview = true;
        state.permissions.canQueueNonMoveActions = true;
        state.hasBuildPreview = true;
        state.buildPreviewType = BuildingType::Barracks;
        state.buildPreviewAnchorCell = {12, 12};
        state.buildPreviewRotationQuarterTurns = 0;

        const TurnCommand pendingBuild = makeBuildCommand(BuildingType::Barracks, {11, 11});
        const std::vector<sf::Vector2i> buildableAnchors{{12, 12}};
        const std::vector<sf::Vector2i> buildableCells{{12, 12}, {13, 12}};
        const WorldRenderPlan plan = RenderCoordinator::buildWorldRenderPlan(
            state,
            std::vector<TurnCommand>{pendingBuild},
            buildableAnchors,
            buildableCells,
            config);

        expect(plan.liveBuildPreview.has_value(),
            "RenderCoordinator should expose the live build preview when build-preview rendering is allowed.");
        expect(plan.liveBuildPreview->valid,
            "RenderCoordinator should mark the live build preview valid when its anchor cell is currently buildable.");
        expect(plan.highlightedCells.size() == buildableCells.size(),
            "RenderCoordinator should surface buildable overlay cells while the build tool is active.");
        expect(plan.pendingBuildPreviews.size() == 1,
            "RenderCoordinator should expose queued build previews when pending commands are rendered concretely as overlays.");
        expect(plan.actionMarkers.size() == 1
                && plan.actionMarkers.front().iconName == "build_ongoing",
            "RenderCoordinator should add build action markers for queued build commands.");
    }

    void testUpdateCoordinatorPlansPlayingTick() {
        const FrameUpdatePlan plan = UpdateCoordinator::planFrameUpdate(FrameUpdateState{
            GameState::Playing,
            true,
            true,
            false,
            true
        });

        expect(plan.syncTurnDraft,
            "UpdateCoordinator should keep turn-draft synchronization enabled on every gameplay tick.");
        expect(plan.updateCamera,
            "UpdateCoordinator should allow camera movement when permissions keep it enabled.");
        expect(plan.updateMultiplayer,
            "UpdateCoordinator should tick multiplayer while the game is actively playing.");
        expect(plan.runAITurn,
            "UpdateCoordinator should plan AI polling during playing ticks when the active kingdom is AI-controlled.");
        expect(plan.updateUI && plan.updateUIManager,
            "UpdateCoordinator should refresh both in-game UI state and UI animations during playing ticks.");
    }

    void testUpdateCoordinatorPlansPausedAndMenuTicks() {
        const FrameUpdatePlan pausedPlan = UpdateCoordinator::planFrameUpdate(FrameUpdateState{
            GameState::Paused,
            false,
            false,
            false,
            true
        });
        expect(!pausedPlan.runAITurn,
            "UpdateCoordinator should never run AI ticks while the game is paused.");
        expect(pausedPlan.updateUI && pausedPlan.updateUIManager,
            "UpdateCoordinator should keep paused HUD and menu UI refreshed.");

        const FrameUpdatePlan menuLanPlan = UpdateCoordinator::planFrameUpdate(FrameUpdateState{
            GameState::MainMenu,
            false,
            false,
            true,
            false
        });
        expect(menuLanPlan.updateMultiplayer,
            "UpdateCoordinator should keep LAN client transport updating even outside active gameplay states.");
        expect(!menuLanPlan.updateUI && !menuLanPlan.updateUIManager,
            "UpdateCoordinator should not run in-game UI refreshes while the game is in the main menu.");
    }

    void testAITurnCoordinatorPlansStartAndCompletion() {
        AITurnRuntimeState runtimeState;
        runtimeState.gameState = GameState::Playing;
        runtimeState.activeAI = true;
        runtimeState.activeKingdom = KingdomId::Black;
        runtimeState.turnNumber = 7;

        const AITurnStartPlan startPlan = AITurnCoordinator::buildStartPlan(runtimeState);
        expect(startPlan.shouldStart,
            "AITurnCoordinator should start AI planning when gameplay is active, an AI turn is active, and no runner is busy.");
        expect(startPlan.activeKingdom == KingdomId::Black && startPlan.turnNumber == 7,
            "AITurnCoordinator should preserve the active kingdom and turn number when starting a new AI planning task.");

        runtimeState.runnerBusy = true;
        expect(!AITurnCoordinator::buildStartPlan(runtimeState).shouldStart,
            "AITurnCoordinator should not start a second AI planning task while the runner is already busy.");

        runtimeState.runnerBusy = false;
        runtimeState.gameState = GameState::Paused;
        expect(!AITurnCoordinator::buildStartPlan(runtimeState).shouldStart,
            "AITurnCoordinator should not start AI planning outside active gameplay.");

        runtimeState.gameState = GameState::Playing;
        AITurnRunner::CompletedTurn completedTurn;
        completedTurn.activeKingdom = KingdomId::Black;
        completedTurn.turnNumber = 7;
        completedTurn.plan.objectiveName = "Pressure Net";

        TurnCommand moveCommand;
        moveCommand.type = TurnCommand::Move;
        moveCommand.pieceId = 31;
        moveCommand.origin = {4, 5};
        moveCommand.destination = {5, 5};

        TurnCommand buildCommand;
        buildCommand.type = TurnCommand::Build;
        buildCommand.buildingType = BuildingType::Barracks;
        buildCommand.buildOrigin = {9, 10};

        TurnCommand produceCommand;
        produceCommand.type = TurnCommand::Produce;
        produceCommand.barracksId = 22;
        produceCommand.produceType = PieceType::Knight;

        completedTurn.plan.commands = {moveCommand, buildCommand, produceCommand};

        const AITurnCompletionPlan completionPlan = AITurnCoordinator::buildCompletionPlan(
            runtimeState,
            completedTurn);
        expect(completionPlan.applyPlanMetadata
                && completionPlan.printDebugSummary
                && completionPlan.stageTurn
                && completionPlan.logObjective
                && completionPlan.logPlanningComplete
                && completionPlan.commitAuthoritativeTurn,
            "AITurnCoordinator should fully apply a completed AI plan when it still matches the active gameplay turn.");
        expect(completionPlan.objectiveName == "Pressure Net"
                && completionPlan.activeKingdom == KingdomId::Black
                && completionPlan.turnNumber == 7,
            "AITurnCoordinator should preserve the completed AI objective and authoritative turn identity.");
        expect(completionPlan.debugLines.size() == 3,
            "AITurnCoordinator should expose one debug summary line per AI command.");
        expect(completionPlan.debugLines[0].find("Move piece=31") != std::string::npos,
            "AITurnCoordinator should describe queued AI move commands in the debug summary.");
        expect(completionPlan.debugLines[1].find("Barracks") != std::string::npos,
            "AITurnCoordinator should describe queued AI build commands in the debug summary.");
        expect(completionPlan.debugLines[2].find("Knight") != std::string::npos,
            "AITurnCoordinator should describe queued AI production commands in the debug summary.");

        runtimeState.turnNumber = 8;
        const AITurnCompletionPlan stalePlan = AITurnCoordinator::buildCompletionPlan(runtimeState, completedTurn);
        expect(stalePlan.shouldIgnore,
            "AITurnCoordinator should ignore completed AI plans that no longer match the authoritative turn.");
        expect(!stalePlan.commitAuthoritativeTurn,
            "AITurnCoordinator should not request an authoritative commit for stale AI results.");
    }

    void testTurnCoordinatorBuildsCommitPlans() {
        AuthoritativeTurnExecution pendingCheckmate;
        pendingCheckmate.gameOver = true;
        pendingCheckmate.winner = KingdomId::White;

        const AuthoritativeCommitPlan pendingCheckmatePlan = TurnCoordinator::buildAuthoritativeCommitPlan(
            pendingCheckmate,
            true,
            "White Player");
        expect(pendingCheckmatePlan.nextGameState.has_value()
                && *pendingCheckmatePlan.nextGameState == GameState::GameOver,
            "TurnCoordinator should switch the runtime into game-over state when an uncommitted pending turn is already checkmate.");
        expect(pendingCheckmatePlan.eventLogEntry.has_value()
                && pendingCheckmatePlan.eventLogEntry->second.find("White Player wins") != std::string::npos,
            "TurnCoordinator should expose the winner event log entry for checkmate commit plans.");
        expect(pendingCheckmatePlan.persistLanHostSnapshot,
            "TurnCoordinator should request host persistence when a LAN host reaches checkmate.");
        expect(pendingCheckmatePlan.updateUI,
            "TurnCoordinator should refresh the HUD immediately after a terminal checkmate outcome.");

        AuthoritativeTurnExecution committedTurn;
        committedTurn.committed = true;
        const AuthoritativeCommitPlan committedPlan = TurnCoordinator::buildAuthoritativeCommitPlan(
            committedTurn,
            false,
            "White Player");
        expect(committedPlan.clearMovePreview
                && committedPlan.clearWaitingForRemoteTurnResult
                && committedPlan.refreshTurnPhase,
            "TurnCoordinator should clear local pending-turn presentation state after a successful authoritative commit.");
        expect(committedPlan.syncTurnDraftBeforeReconcile && committedPlan.reconcileSelection,
            "TurnCoordinator should request turn-draft resync before restoring the selection bookmark after a successful commit.");
        expect(committedPlan.startAITurn,
            "TurnCoordinator should request the next AI turn after a successful non-terminal authoritative commit.");
    }

    void testTurnCoordinatorRejectsAndAcceptsNetworkTurnFlow() {
        GameConfig config;
        GameEngine engine;
        MultiplayerRuntime multiplayer;
        GameStateDebugRecorder debugRecorder;
        TurnCoordinator coordinator(engine, multiplayer, debugRecorder, config);

        GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman,
                                                                 "turn_coordinator_network_test");
        std::string error;
        expect(engine.startNewSession(session, config, &error), error);

        const ClientTurnSubmissionResult unauthenticatedSubmit = coordinator.submitClientTurn(true);
        expect(!unauthenticatedSubmit.submitted,
            "TurnCoordinator should reject client submissions when the LAN client transport is not authenticated.");
        expect(unauthenticatedSubmit.errorMessage.find("not authenticated") != std::string::npos,
            "TurnCoordinator should expose the authentication failure reason for client turn submission.");

        const RemoteTurnSubmissionResult notHostResult = coordinator.applyRemoteTurnSubmission(false, {});
        expect(!notHostResult.accepted,
            "TurnCoordinator should reject remote turn application outside LAN host mode.");
        expect(notHostResult.rejectionMessage.find("outside LAN host mode") != std::string::npos,
            "TurnCoordinator should explain why remote turn application was rejected outside host mode.");

        engine.turnSystem().setActiveKingdom(KingdomId::Black);
        const RemoteTurnSubmissionResult acceptedResult = coordinator.applyRemoteTurnSubmission(true, {});
        expect(acceptedResult.accepted,
            "TurnCoordinator should accept a legal empty remote turn submission for Black when hosted authoritatively.");
        expect(acceptedResult.shouldCommitAuthoritativeTurn,
            "TurnCoordinator should request an authoritative commit after a valid remote turn submission.");
        expect(!acceptedResult.shouldResetPendingCommands,
            "TurnCoordinator should not request pending-turn reset after an accepted remote turn submission.");
    }

    void testTurnLifecycleCoordinatorBuildsDispatchAndResetPlans() {
        const CommitPlayerTurnDispatchPlan localDispatchPlan =
            TurnLifecycleCoordinator::buildCommitPlayerTurnDispatchPlan(false);
        expect(localDispatchPlan.commitAuthoritativeTurn && !localDispatchPlan.submitClientTurn,
            "TurnLifecycleCoordinator should route non-client end-turn requests directly to authoritative commit.");

        const CommitPlayerTurnDispatchPlan lanClientDispatchPlan =
            TurnLifecycleCoordinator::buildCommitPlayerTurnDispatchPlan(true);
        expect(lanClientDispatchPlan.submitClientTurn && !lanClientDispatchPlan.commitAuthoritativeTurn,
            "TurnLifecycleCoordinator should route LAN client end-turn requests through remote submission.");

        const ClientTurnSubmissionFailurePlan connectedFailurePlan =
            TurnLifecycleCoordinator::buildClientTurnSubmissionFailurePlan("Timed out", true);
        expect(connectedFailurePlan.showAlert,
            "TurnLifecycleCoordinator should surface a turn-not-sent alert when a connected LAN client submission fails with an error message.");
        expect(connectedFailurePlan.alertTitle == "Turn Not Sent"
                && connectedFailurePlan.alertMessage == "Timed out",
            "TurnLifecycleCoordinator should preserve the submission failure message in the LAN client alert plan.");

        const ClientTurnSubmissionFailurePlan disconnectedFailurePlan =
            TurnLifecycleCoordinator::buildClientTurnSubmissionFailurePlan("Timed out", false);
        expect(!disconnectedFailurePlan.showAlert,
            "TurnLifecycleCoordinator should suppress the turn-not-sent alert when the LAN client is already disconnected.");

        const ClientTurnSubmissionFailurePlan emptyFailurePlan =
            TurnLifecycleCoordinator::buildClientTurnSubmissionFailurePlan("", true);
        expect(!emptyFailurePlan.showAlert,
            "TurnLifecycleCoordinator should suppress the turn-not-sent alert when no concrete failure message is available.");

        const TurnResetPlan resetPlan = TurnLifecycleCoordinator::buildResetPlan();
        expect(resetPlan.cancelLiveMove
                && resetPlan.resetPendingCommands
                && resetPlan.syncTurnDraftBeforeReconcile
                && resetPlan.reconcileSelection,
            "TurnLifecycleCoordinator should keep reset-turn flow aligned with live-move cancel, pending-command clear, draft resync, and selection restore.");
    }

    void testMultiplayerJoinCoordinatorPreparesReconnectAndRejectsInvalidJoin() {
        GameConfig config;
        GameEngine engine;
        MultiplayerRuntime runtime;
        SaveManager saveManager;
        InputHandler input;
        MultiplayerJoinCoordinator coordinator(engine, runtime, saveManager, input, config);

        JoinMultiplayerRequest reconnectRequest;
        std::string error;
        expect(!MultiplayerJoinCoordinator::buildReconnectJoinRequest(runtime, reconnectRequest, &error),
            "MultiplayerJoinCoordinator should reject reconnect requests when no previous host is cached.");
        expect(error.find("No previous multiplayer host") != std::string::npos,
            "MultiplayerJoinCoordinator should explain missing reconnect context.");

        runtime.cacheReconnectRequest(MultiplayerJoinCredentials{"127.0.0.1", 4242, "secret"});
        error.clear();
        expect(MultiplayerJoinCoordinator::buildReconnectJoinRequest(runtime, reconnectRequest, &error),
            "MultiplayerJoinCoordinator should rebuild a join request from cached reconnect credentials.");
        expect(reconnectRequest.host == "127.0.0.1"
                && reconnectRequest.port == 4242
                && reconnectRequest.password == "secret",
            "MultiplayerJoinCoordinator should preserve reconnect host, port and password.");

        LocalPlayerContext localContext = makeLanClientLocalPlayerContext();
        bool waitingForRemoteTurn = true;
        input.setTool(ToolState::Build);
        const MultiplayerJoinPreparationPlan preparationPlan = coordinator.prepareForClientConnectionAttempt(
            localContext,
            waitingForRemoteTurn,
            false);
        expect(preparationPlan.invalidateTurnDraft
                && preparationPlan.hideGameMenu
                && preparationPlan.hideMultiplayerAlert
                && preparationPlan.hideMultiplayerWaitingOverlay
                && preparationPlan.clearMultiplayerStatus,
            "MultiplayerJoinCoordinator should request the expected UI cleanup before a client connection attempt.");
        expect(!waitingForRemoteTurn,
            "MultiplayerJoinCoordinator should clear the waiting-for-remote-turn flag before attempting a new client connection.");
        expect(localContext.mode == LocalSessionMode::LocalOnly,
            "MultiplayerJoinCoordinator should clear LAN client local-control context when the caller does not preserve it.");
        expect(input.getCurrentTool() == ToolState::Select,
            "MultiplayerJoinCoordinator should reset the input tool back to selection before joining a host.");

        LocalPlayerContext preservedContext = makeLanClientLocalPlayerContext();
        bool preservedWaiting = true;
        coordinator.prepareForClientConnectionAttempt(preservedContext, preservedWaiting, true);
        expect(preservedContext.mode == LocalSessionMode::LanClient,
            "MultiplayerJoinCoordinator should preserve the LAN client control context during reconnect attempts.");

        const MultiplayerJoinPresentationPlan joinPlan = coordinator.joinMultiplayer(
            JoinMultiplayerRequest{"not_an_ip", 4242, "secret"},
            preservedContext,
            preservedWaiting);
        expect(!joinPlan.joined,
            "MultiplayerJoinCoordinator should surface join failures instead of pretending the client joined.");
        expect(joinPlan.errorMessage.find("Invalid server IP address") != std::string::npos,
            "MultiplayerJoinCoordinator should forward multiplayer join errors from the runtime.");
    }

    void testInGamePresentationCoordinatorPlansMenuTransitions() {
        FrontendRuntimeState localState;
        localState.gameState = GameState::Playing;

        const InGameMenuOpenPlan localOpenPlan = InGamePresentationCoordinator::planOpenInGameMenu(localState);
        expect(localOpenPlan.shouldOpen,
            "InGamePresentationCoordinator should allow opening the in-game menu during local gameplay.");
        expect(localOpenPlan.nextGameState.has_value()
                && *localOpenPlan.nextGameState == GameState::Paused,
            "InGamePresentationCoordinator should pause local gameplay when opening the in-game menu.");
        expect(localOpenPlan.presentation.pauseState == GameMenuPauseState::Paused,
            "InGamePresentationCoordinator should mark the local in-game menu as paused.");
        expect(localOpenPlan.presentation.showSave,
            "InGamePresentationCoordinator should keep save actions visible for local sessions.");

        FrontendRuntimeState lanClientState;
        lanClientState.gameState = GameState::Playing;
        lanClientState.localPlayerContext = makeLanClientLocalPlayerContext();

        const InGameMenuOpenPlan lanClientOpenPlan = InGamePresentationCoordinator::planOpenInGameMenu(
            lanClientState);
        expect(lanClientOpenPlan.shouldOpen,
            "InGamePresentationCoordinator should still allow opening the in-game menu during LAN client gameplay.");
        expect(!lanClientOpenPlan.nextGameState.has_value(),
            "InGamePresentationCoordinator should not pause the runtime when a networked session opens the menu.");
        expect(lanClientOpenPlan.presentation.pauseState == GameMenuPauseState::NotPaused,
            "InGamePresentationCoordinator should mark networked menus as non-pausing overlays.");
        expect(!lanClientOpenPlan.presentation.showSave,
            "InGamePresentationCoordinator should hide save actions for LAN clients.");

        FrontendRuntimeState mainMenuState;
        mainMenuState.gameState = GameState::MainMenu;
        expect(!InGamePresentationCoordinator::planOpenInGameMenu(mainMenuState).shouldOpen,
            "InGamePresentationCoordinator should reject menu-open requests outside in-game states.");

        FrontendRuntimeState pausedLocalState;
        pausedLocalState.gameState = GameState::Paused;
        const InGameMenuClosePlan localClosePlan = InGamePresentationCoordinator::planCloseInGameMenu(
            pausedLocalState);
        expect(localClosePlan.shouldClose,
            "InGamePresentationCoordinator should allow closing an already-open in-game menu.");
        expect(localClosePlan.nextGameState.has_value()
                && *localClosePlan.nextGameState == GameState::Playing,
            "InGamePresentationCoordinator should resume local gameplay when the paused in-game menu closes.");

        const InGameMenuClosePlan lanClientClosePlan = InGamePresentationCoordinator::planCloseInGameMenu(
            lanClientState);
        expect(lanClientClosePlan.shouldClose,
            "InGamePresentationCoordinator should still close networked in-game menus.");
        expect(!lanClientClosePlan.nextGameState.has_value(),
            "InGamePresentationCoordinator should leave networked runtime state unchanged when closing the menu.");
    }

    void testSessionPresentationCoordinatorPlansSessionEntryAndMenuReturn() {
        const SessionResetPlan resetPlan = SessionPresentationCoordinator::buildAuthoritativeSessionResetPlan();
        expect(resetPlan.stopMultiplayer
                && resetPlan.discardPendingAiTurn
                && resetPlan.clearMovePreview
                && resetPlan.activateSelectTool
                && resetPlan.resetPendingTurn
                && resetPlan.invalidateTurnDraft,
            "SessionPresentationCoordinator should request the full authoritative session reset before start/load transitions.");

        GameSessionConfig localSession = makeDefaultGameSessionConfig(GameMode::HumanVsHuman,
                                                                      "session_presentation_local_test");
        const SessionPresentationPlan localPresentationPlan =
            SessionPresentationCoordinator::buildSessionPresentationPlan(localSession, true);
        expect(localPresentationPlan.localPlayerContext.mode == LocalSessionMode::LocalOnly,
            "SessionPresentationCoordinator should keep local sessions in local-only control mode.");
        expect(localPresentationPlan.cameraKingdom == KingdomId::White,
            "SessionPresentationCoordinator should center the camera on White for the default local session.");
        expect(localPresentationPlan.nextGameState == GameState::Playing
                && localPresentationPlan.refreshTurnPhase
                && localPresentationPlan.showHud
                && localPresentationPlan.updateUI,
            "SessionPresentationCoordinator should request the normal post-load/post-start gameplay presentation updates.");
        expect(localPresentationPlan.saveAuthoritativeSession,
            "SessionPresentationCoordinator should request an initial save after starting a fresh authoritative session.");

        GameSessionConfig lanSession = makeDefaultGameSessionConfig(GameMode::HumanVsHuman,
                                                                    "session_presentation_lan_test");
        lanSession.multiplayer.enabled = true;
        lanSession.multiplayer.port = 4242;
        const SessionPresentationPlan lanPresentationPlan =
            SessionPresentationCoordinator::buildSessionPresentationPlan(lanSession, false);
        expect(lanPresentationPlan.localPlayerContext.mode == LocalSessionMode::LanHost,
            "SessionPresentationCoordinator should restore LAN host local-control context for multiplayer sessions.");
        expect(lanPresentationPlan.cameraKingdom == KingdomId::White,
            "SessionPresentationCoordinator should keep the host camera centered on White after session restore.");
        expect(!lanPresentationPlan.saveAuthoritativeSession,
            "SessionPresentationCoordinator should not force an extra save when only restoring an existing session.");

        const MainMenuPresentationPlan mainMenuPlan = SessionPresentationCoordinator::buildReturnToMainMenuPlan();
        expect(mainMenuPlan.stopMultiplayer
                && mainMenuPlan.discardPendingAiTurn
                && mainMenuPlan.clearMovePreview
                && mainMenuPlan.activateSelectTool
                && mainMenuPlan.resetPendingTurn
                && mainMenuPlan.invalidateTurnDraft,
            "SessionPresentationCoordinator should request a full runtime cleanup before returning to the main menu.");
        expect(mainMenuPlan.nextGameState == GameState::MainMenu
                && mainMenuPlan.hideGameMenu
                && mainMenuPlan.showMainMenu,
            "SessionPresentationCoordinator should restore the main-menu presentation after quitting an in-game session.");
    }

    void testPanelActionCoordinatorQueuesAndCancelsPieceAndBarracksActions() {
        GameConfig config;
        GameEngine engine;
        InputHandler input;
        PanelActionCoordinator coordinator(engine, input, config);

        GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman,
                                                                 "panel_action_coordinator_test");
        std::string error;
        expect(engine.startNewSession(session, config, &error), error);
        engine.activeKingdom().gold = 1000;

        InteractionPermissions permissions;
        permissions.canQueueNonMoveActions = true;

        const sf::Vector2i pawnCell = findEmptyTraversableCell(engine);
        Piece& pawn = addPieceToBoard(engine.activeKingdom(),
                                      engine.board(),
                                      9100,
                                      PieceType::Pawn,
                                      KingdomId::White,
                                      pawnCell);
        pawn.xp = config.getXPThresholdPawnToKnightOrBishop();

        coordinator.handlePieceUpgradeRequest(permissions, pawn.id, PieceType::Knight);
        const TurnCommand* queuedUpgrade = engine.turnSystem().getPendingUpgradeCommand(pawn.id);
        expect(queuedUpgrade != nullptr && queuedUpgrade->upgradeTarget == PieceType::Knight,
            "PanelActionCoordinator should queue an upgrade command for an eligible active piece.");

        coordinator.handlePieceUpgradeRequest(permissions, pawn.id, PieceType::Knight);
        expect(engine.turnSystem().getPendingUpgradeCommand(pawn.id) == nullptr,
            "PanelActionCoordinator should cancel the queued upgrade when the same upgrade is requested twice.");

        coordinator.handlePieceDisbandRequest(permissions, pawn.id);
        expect(engine.turnSystem().getPendingDisbandCommand(pawn.id) != nullptr,
            "PanelActionCoordinator should queue disband for a non-king active piece.");

        coordinator.handlePieceDisbandRequest(permissions, pawn.id);
        expect(engine.turnSystem().getPendingDisbandCommand(pawn.id) == nullptr,
            "PanelActionCoordinator should cancel disband when the same piece is toggled again.");

        const sf::Vector2i barracksOrigin = findEmptyTraversableCell(engine);
        engine.activeKingdom().addBuilding(makeTestBarracks(9200, KingdomId::White, barracksOrigin, config));
        Building* barracks = findBuildingById(engine.activeKingdom(), 9200);
        expect(barracks != nullptr,
            "PanelActionCoordinator test should create an active barracks before queueing production.");
        if (barracks != nullptr) {
            linkBuildingOnBoard(*barracks, engine.board());

            coordinator.handleBarracksProduceRequest(permissions, barracks->id, PieceType::Pawn);
            const TurnCommand* queuedProduce = engine.turnSystem().getPendingProduceCommand(barracks->id);
            expect(queuedProduce != nullptr && queuedProduce->produceType == PieceType::Pawn,
                "PanelActionCoordinator should queue production for the active barracks.");

            coordinator.handleBarracksProduceRequest(permissions, barracks->id, PieceType::Pawn);
            expect(engine.turnSystem().getPendingProduceCommand(barracks->id) == nullptr,
                "PanelActionCoordinator should cancel production when the same barracks order is toggled twice.");
        }
    }

    void testSessionRuntimeCoordinatorAppliesSessionEntryAndMainMenuTransitions() {
        GameConfig config;
        GameEngine engine;
        SaveManager saveManager;
        MultiplayerRuntime multiplayer;
        GameStateDebugRecorder debugRecorder;
        InputHandler input;
        const auto tempDir = std::filesystem::temp_directory_path()
            / ("anormalchessgame_sessionruntime_"
                + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(tempDir);

        try {
            SessionFlow flow(engine,
                             saveManager,
                             multiplayer,
                             debugRecorder,
                             config,
                             tempDir.string());
            MultiplayerJoinCoordinator joinCoordinator(engine, multiplayer, saveManager, input, config);

            GameState gameState = GameState::Paused;
            bool waitingForRemoteTurnResult = true;
            LocalPlayerContext localPlayerContext = makeLanClientLocalPlayerContext();
            SessionRuntimeCoordinator coordinator(gameState,
                                                 waitingForRemoteTurnResult,
                                                 localPlayerContext,
                                                 engine,
                                                 multiplayer,
                                                 flow,
                                                 joinCoordinator);

            bool stopMultiplayer = false;
            bool discardPendingAiTurn = false;
            bool clearMovePreview = false;
            bool activateSelectTool = false;
            bool resetPendingTurn = false;
            bool resetPendingCommands = false;
            bool invalidateTurnDraft = false;
            bool centerCamera = false;
            bool refreshTurnPhase = false;
            bool showHud = false;
            bool updateUi = false;
            bool saveGame = false;
            bool reconcileSelection = false;
            bool hideGameMenu = false;
            bool hideMultiplayerAlert = false;
            bool hideMultiplayerWaitingOverlay = false;
            bool clearMultiplayerStatus = false;
            bool showMainMenu = false;
            KingdomId centeredKingdom = KingdomId::Black;

            SessionRuntimeCallbacks callbacks{
                [&stopMultiplayer]() {
                    stopMultiplayer = true;
                },
                [&discardPendingAiTurn]() {
                    discardPendingAiTurn = true;
                },
                [&clearMovePreview]() {
                    clearMovePreview = true;
                },
                [&activateSelectTool]() {
                    activateSelectTool = true;
                },
                [&resetPendingTurn]() {
                    resetPendingTurn = true;
                },
                [&resetPendingCommands]() {
                    resetPendingCommands = true;
                },
                [&invalidateTurnDraft]() {
                    invalidateTurnDraft = true;
                },
                [&centerCamera, &centeredKingdom](KingdomId kingdom) {
                    centerCamera = true;
                    centeredKingdom = kingdom;
                },
                [&refreshTurnPhase]() {
                    refreshTurnPhase = true;
                },
                [&showHud]() {
                    showHud = true;
                },
                [&updateUi]() {
                    updateUi = true;
                },
                [&saveGame]() {
                    saveGame = true;
                },
                []() {
                    return InputSelectionBookmark{};
                },
                [&reconcileSelection](const InputSelectionBookmark&) {
                    reconcileSelection = true;
                },
                [&hideGameMenu]() {
                    hideGameMenu = true;
                },
                [&hideMultiplayerAlert]() {
                    hideMultiplayerAlert = true;
                },
                [&hideMultiplayerWaitingOverlay]() {
                    hideMultiplayerWaitingOverlay = true;
                },
                [&clearMultiplayerStatus]() {
                    clearMultiplayerStatus = true;
                },
                [&showMainMenu]() {
                    showMainMenu = true;
                }};

            coordinator.returnToMainMenu(callbacks);
            expect(gameState == GameState::MainMenu,
                "SessionRuntimeCoordinator should switch the runtime back to the main menu state when quitting a session.");
            expect(stopMultiplayer
                    && discardPendingAiTurn
                    && clearMovePreview
                    && activateSelectTool
                    && resetPendingCommands
                    && invalidateTurnDraft,
                "SessionRuntimeCoordinator should apply the full main-menu cleanup plan before leaving a session.");
            expect(hideGameMenu && showMainMenu,
                "SessionRuntimeCoordinator should restore the main-menu presentation after quitting an in-game session.");

            stopMultiplayer = false;
            discardPendingAiTurn = false;
            clearMovePreview = false;
            activateSelectTool = false;
            resetPendingTurn = false;
            resetPendingCommands = false;
            invalidateTurnDraft = false;
            centerCamera = false;
            refreshTurnPhase = false;
            showHud = false;
            updateUi = false;
            saveGame = false;
            reconcileSelection = false;
            hideGameMenu = false;
            hideMultiplayerAlert = false;
            hideMultiplayerWaitingOverlay = false;
            clearMultiplayerStatus = false;
            showMainMenu = false;
            centeredKingdom = KingdomId::Black;
            waitingForRemoteTurnResult = true;
            localPlayerContext = makeLanClientLocalPlayerContext();

            GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman,
                                                                     "session_runtime_transition_test");
            std::string error;
            expect(coordinator.startNewGame(session, callbacks, &error), error);
            expect(resetPendingTurn && invalidateTurnDraft,
                "SessionRuntimeCoordinator should apply the authoritative session reset before starting a new session.");
            expect(gameState == GameState::Playing,
                "SessionRuntimeCoordinator should transition into active gameplay after starting a new session.");
            expect(localPlayerContext.mode == LocalSessionMode::LocalOnly,
                "SessionRuntimeCoordinator should restore local-only control context for a fresh local session.");
            expect(!waitingForRemoteTurnResult,
                "SessionRuntimeCoordinator should clear the waiting-for-remote-turn flag after entering a fresh session.");
            expect(centerCamera && centeredKingdom == KingdomId::White,
                "SessionRuntimeCoordinator should center the camera on White after entering a default local session.");
            expect(refreshTurnPhase && showHud && updateUi && saveGame,
                "SessionRuntimeCoordinator should apply the expected HUD, phase, UI and save callbacks after starting a new session.");
        } catch (...) {
            std::filesystem::remove_all(tempDir);
            throw;
        }
    }

    void testUICallbackCoordinatorGuardsHudAndToolbarActions() {
        GameConfig config;
        GameEngine engine;
        SaveManager saveManager;
        InputHandler input;
        PanelActionCoordinator panelActionCoordinator(engine, input, config);
        UIManager uiManager;
        Kingdom whiteKingdom(KingdomId::White);
        Kingdom blackKingdom(KingdomId::Black);

        UICallbackRuntimeState runtimeState;
        runtimeState.gameState = GameState::MainMenu;

        int toggleMenuCalls = 0;
        int resetTurnCalls = 0;
        int commitTurnCalls = 0;
        int activateSelectToolCalls = 0;
        int toggleOverviewCalls = 0;

        const UICallbackBindings bindings = UICallbackCoordinator::buildBindings(
            UICallbackCoordinatorDependencies{
                [&runtimeState]() {
                    return runtimeState;
                },
                uiManager,
                saveManager,
                input,
                panelActionCoordinator,
                config,
                []() {},
                []() {},
                []() {},
                [](const GameSessionConfig&, std::string*) {
                    return true;
                },
                [](const std::string&) {},
                [](const JoinMultiplayerRequest&, std::string*) {
                    return true;
                },
                []() {},
                []() {},
                [&toggleMenuCalls]() {
                    ++toggleMenuCalls;
                },
                [&resetTurnCalls]() {
                    ++resetTurnCalls;
                },
                [&commitTurnCalls]() {
                    ++commitTurnCalls;
                },
                [&activateSelectToolCalls]() {
                    ++activateSelectToolCalls;
                },
                [&toggleOverviewCalls]() {
                    ++toggleOverviewCalls;
                },
                []() {},
                []() {
                    return KingdomId::White;
                },
                []() {
                    return KingdomId::White;
                },
                [&whiteKingdom, &blackKingdom](KingdomId id) -> Kingdom& {
                    return id == KingdomId::White ? whiteKingdom : blackKingdom;
                }});

        bindings.hud.onMenu();
        bindings.hud.onResetTurn();
        bindings.hud.onEndTurn();
        bindings.toolBar.onSelect();
        bindings.toolBar.onOverview();
        expect(toggleMenuCalls == 0
                && resetTurnCalls == 0
                && commitTurnCalls == 0
                && activateSelectToolCalls == 0
                && toggleOverviewCalls == 0,
            "UICallbackCoordinator should block HUD and toolbar actions while gameplay state or permissions do not allow them.");

        runtimeState.gameState = GameState::Playing;
        runtimeState.permissions.canIssueCommands = true;
        runtimeState.permissions.canUseToolbar = true;

        bindings.hud.onMenu();
        bindings.hud.onResetTurn();
        bindings.hud.onEndTurn();
        bindings.toolBar.onSelect();
        bindings.toolBar.onOverview();
        expect(toggleMenuCalls == 1,
            "UICallbackCoordinator should forward the HUD menu action during interactive gameplay states.");
        expect(resetTurnCalls == 1 && commitTurnCalls == 1,
            "UICallbackCoordinator should forward reset/end-turn HUD actions when the local player can issue commands.");
        expect(activateSelectToolCalls == 1 && toggleOverviewCalls == 1,
            "UICallbackCoordinator should forward toolbar actions when toolbar interaction is allowed.");
    }

    void testSelectionQueryCoordinatorResolvesBookmarkFallback() {
        GameConfig config;
        std::array<Kingdom, kNumKingdoms> kingdoms{Kingdom(KingdomId::White), Kingdom(KingdomId::Black)};
        std::vector<Building> publicBuildings;

        Building trackedBuilding = makeTestBarracks(41, KingdomId::White, {6, 4}, config);
        trackedBuilding.rotationQuarterTurns = 1;
        kingdoms[0].buildings.push_back(trackedBuilding);

        InputSelectionBookmark bookmark;
        bookmark.buildingId = 999;
        bookmark.selectedBuildingOrigin = trackedBuilding.origin;
        bookmark.selectedBuildingType = trackedBuilding.type;
        bookmark.selectedBuildingOwner = trackedBuilding.owner;
        bookmark.selectedBuildingIsNeutral = trackedBuilding.isNeutral;
        bookmark.selectedBuildingRotationQuarterTurns = trackedBuilding.rotationQuarterTurns;

        const SelectionQueryView view{kingdoms, publicBuildings};
        Building* resolvedByBookmark = SelectionQueryCoordinator::findBuildingForBookmark(view, bookmark);
        expect(resolvedByBookmark == &kingdoms[0].buildings.front(),
            "SelectionQueryCoordinator should recover a building selection from bookmark metadata when the cached building id is stale.");
        expect(SelectionQueryCoordinator::findBuildingById(view, trackedBuilding.id) == &kingdoms[0].buildings.front(),
            "SelectionQueryCoordinator should still resolve displayed building selections directly by stable id.");
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

void testGameEngineRestoresBishopSpawnMemory() {
    GameConfig config;
    GameEngine engine;
    GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "engine_bishop_memory_restore_test");

    std::string error;
    expect(engine.startNewSession(session, config, &error), error);
    engine.kingdom(KingdomId::White).hasSpawnedBishop = true;
    engine.kingdom(KingdomId::White).lastBishopSpawnParity = 1;

    const SaveData save = engine.createSaveData();

    GameEngine restored;
    expect(restored.restoreFromSave(save, config, &error), error);
    expect(restored.kingdom(KingdomId::White).hasSpawnedBishop,
           "Restoring save data should preserve bishop spawn history per kingdom.");
    expect(restored.kingdom(KingdomId::White).lastBishopSpawnParity == 1,
           "Restoring save data should preserve the last bishop spawn parity.");
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

    void testGameEngineStagesAITurnFallbackMoveWhenInCheck() {
        GameConfig config;
        GameEngine engine;
        engine.board().init(12);
        engine.kingdom(KingdomId::White) = Kingdom(KingdomId::White);
        engine.kingdom(KingdomId::Black) = Kingdom(KingdomId::Black);
        engine.publicBuildings().clear();
        engine.turnSystem().setActiveKingdom(KingdomId::White);
        engine.turnSystem().setTurnNumber(1);

        Kingdom& white = engine.kingdom(KingdomId::White);
        Kingdom& black = engine.kingdom(KingdomId::Black);
        addPieceToBoard(white, engine.board(), 1000, PieceType::King, KingdomId::White, {12, 12});
        addPieceToBoard(white, engine.board(), 1001, PieceType::Pawn, KingdomId::White, {11, 11});
        addPieceToBoard(black, engine.board(), 2000, PieceType::King, KingdomId::Black, {4, 4});
        addPieceToBoard(black, engine.board(), 2001, PieceType::Rook, KingdomId::Black, {12, 4});

        const PendingTurnStagingResult stagingResult = engine.stageAITurnPlan(
         {makeBuildCommand(BuildingType::Farm, {6, 6})},
         config);

        expect(stagingResult.usedFallbackResponseMove,
            "AI turn staging should queue a legal fallback move when the active king starts in check.");
        expect(stagingResult.validation.valid,
            "The fallback move should leave a valid pending turn when a legal response exists.");
        expect(!engine.turnSystem().getPendingCommands().empty(),
            "The fallback path should leave a concrete pending command queued.");
        expect(engine.turnSystem().getPendingCommands().front().type == TurnCommand::Move,
            "The fallback command queued from check should be a move.");
    }

    void testGameEngineStagesAITurnDisbandsToResolveBankruptcy() {
        GameConfig config;
        GameEngine engine;
        engine.board().init(12);
        engine.kingdom(KingdomId::White) = Kingdom(KingdomId::White);
        engine.kingdom(KingdomId::Black) = Kingdom(KingdomId::Black);
        engine.publicBuildings().clear();
        engine.turnSystem().setActiveKingdom(KingdomId::White);
        engine.turnSystem().setTurnNumber(1);

        Kingdom& white = engine.kingdom(KingdomId::White);
        Kingdom& black = engine.kingdom(KingdomId::Black);
        addPieceToBoard(white, engine.board(), 1010, PieceType::King, KingdomId::White, {12, 12});
        addPieceToBoard(white, engine.board(), 1011, PieceType::Rook, KingdomId::White, {11, 12});
        addPieceToBoard(white, engine.board(), 1012, PieceType::Knight, KingdomId::White, {10, 12});
        addPieceToBoard(black, engine.board(), 2010, PieceType::King, KingdomId::Black, {4, 4});
        white.gold = 0;

        const int stagedUpkeep = config.getPieceUpkeepCost(PieceType::Rook)
         + config.getPieceUpkeepCost(PieceType::Knight);
        expect(stagedUpkeep > 0,
            "The bankruptcy recovery test requires at least one upkeep-bearing non-king piece.");

        const PendingTurnStagingResult stagingResult = engine.stageAITurnPlan({}, config);

        expect(stagingResult.usedBankruptcyDisbands,
            "AI turn staging should queue emergency disbands when the planned end-of-turn state is bankrupt.");
        expect(stagingResult.validation.valid,
            "Emergency disbands should recover a valid pending turn when enough upkeep can be removed.");
        expect(!engine.turnSystem().getPendingCommands().empty(),
            "Bankruptcy recovery should leave at least one queued disband command.");
        expect(std::all_of(engine.turnSystem().getPendingCommands().begin(),
                  engine.turnSystem().getPendingCommands().end(),
                  [](const TurnCommand& command) {
                      return command.type == TurnCommand::Disband;
                  }),
            "Bankruptcy recovery should only queue disband commands after resetting the invalid plan.");
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

    void testBoardGeneratorAppliesDeterministicGrassBrightnessVariation() {
        GameConfig config;

        Board boardA;
        boardA.init(25);
        std::vector<Building> buildingsA;
        BoardGenerator::generate(boardA, config, buildingsA, 424242u);

        Board boardB;
        boardB.init(25);
        std::vector<Building> buildingsB;
        BoardGenerator::generate(boardB, config, buildingsB, 424242u);

        Board boardC;
        boardC.init(25);
        std::vector<Building> buildingsC;
        BoardGenerator::generate(boardC, config, buildingsC, 424243u);

        int darkerGrassCells = 0;
        int differentBrightnessCells = 0;
        const int diameter = boardA.getDiameter();
        for (int y = 0; y < diameter; ++y) {
            for (int x = 0; x < diameter; ++x) {
                const Cell& cellA = boardA.getCell(x, y);
                const Cell& cellB = boardB.getCell(x, y);
                const Cell& cellC = boardC.getCell(x, y);
                if (!cellA.isInCircle) {
                    continue;
                }

                expect(cellA.terrainBrightness == cellB.terrainBrightness,
                    "Grass brightness should be deterministic for a given world seed.");

                if (cellA.type == CellType::Grass && cellA.terrainBrightness < 255) {
                    ++darkerGrassCells;
                }
                if (cellA.terrainBrightness != cellC.terrainBrightness) {
                    ++differentBrightnessCells;
                }
            }
        }

        expect(darkerGrassCells > 0,
            "World generation should darken at least some grass cells for visual variation.");
        expect(differentBrightnessCells > 0,
            "Changing the world seed should alter at least some terrain brightness values.");
    }

    void testBoardGeneratorCentersChurchWithoutRotation() {
        GameConfig config;

        Board board;
        board.init(25);
        std::vector<Building> publicBuildings;
        BoardGenerator::generate(board, config, publicBuildings, 987654u);

        const Building* church = nullptr;
        for (const Building& building : publicBuildings) {
            if (building.type == BuildingType::Church) {
                church = &building;
                break;
            }
        }

        expect(church != nullptr,
            "World generation should always place a church.");
        expect(church->rotationQuarterTurns == 0,
            "The church should no longer spawn with a random rotation.");
        expect(church->flipMask == 0,
            "The church should no longer spawn with a random flip state.");

        const sf::Vector2i expectedOrigin{
            board.getRadius() - (church->getFootprintWidth() / 2),
            board.getRadius() - (church->getFootprintHeight() / 2)};
        expect(church->origin == expectedOrigin,
            "The church should be centered on the map footprint.");
    }

    void testBoardGeneratorDispersesPublicResourcesAcrossTypes() {
        GameConfig config;
        const std::vector<std::uint32_t> seeds{1337u, 7331u, 424242u, 987654u};

        for (const std::uint32_t seed : seeds) {
            Board board;
            board.init(25);
            std::vector<Building> publicBuildings;
            BoardGenerator::generate(board, config, publicBuildings, seed);

            std::vector<const Building*> mines;
            std::vector<const Building*> farms;
            std::vector<const Building*> resources;
            for (const Building& building : publicBuildings) {
                if (building.type == BuildingType::Mine) {
                    mines.push_back(&building);
                    resources.push_back(&building);
                } else if (building.type == BuildingType::Farm) {
                    farms.push_back(&building);
                    resources.push_back(&building);
                }
            }

            expect(mines.size() == static_cast<std::size_t>(config.getNumMines()),
                "Generated worlds should still contain the configured number of mines.");
            expect(farms.size() == static_cast<std::size_t>(config.getNumFarms()),
                "Generated worlds should still contain the configured number of farms.");

            const float minePairDistance = buildingCenterDistance(*mines[0], *mines[1]);
            float minMineToFarmDistance = std::numeric_limits<float>::max();
            for (const Building* mine : mines) {
                for (const Building* farm : farms) {
                    minMineToFarmDistance = std::min(
                        minMineToFarmDistance,
                        buildingCenterDistance(*mine, *farm));
                }
            }

            expect(minePairDistance >= minMineToFarmDistance,
                "Representative seeds should not cluster both mines more tightly than the nearest mine-to-farm pairing.");

            int crossTypeNearestCount = 0;
            for (const Building* resource : resources) {
                const Building* nearest = nullptr;
                float nearestDistance = std::numeric_limits<float>::max();
                for (const Building* other : resources) {
                    if (resource == other) {
                        continue;
                    }

                    const float distance = buildingCenterDistance(*resource, *other);
                    if (distance < nearestDistance) {
                        nearestDistance = distance;
                        nearest = other;
                    }
                }

                if (nearest && nearest->type != resource->type) {
                    ++crossTypeNearestCount;
                }
            }

            expect(crossTypeNearestCount >= 2,
                "Representative seeds should mix mines and farms instead of producing only same-type local clusters.");
        }
    }

void testSaveManagerRoundTrip() {
    SaveData data;
    data.gameName = "save_roundtrip";
    data.turnNumber = 7;
    data.activeKingdom = KingdomId::Black;
    data.mapRadius = 5;
    data.worldSeed = 123456789u;
    data.weatherSystemState.nextSpawnTurnStep = 19;
    data.weatherSystemState.hasActiveFront = true;
    data.weatherSystemState.rngCounter = 6u;
    data.weatherSystemState.activeFront.direction = WeatherDirection::SouthWest;
    data.weatherSystemState.activeFront.currentTurnStep = 3;
    data.weatherSystemState.activeFront.totalTurnSteps = 11;
    data.weatherSystemState.activeFront.centerStartXTimes1000 = -1750;
    data.weatherSystemState.activeFront.centerStartYTimes1000 = 24500;
    data.weatherSystemState.activeFront.stepXTimes1000 = -610;
    data.weatherSystemState.activeFront.stepYTimes1000 = 610;
    data.weatherSystemState.activeFront.radiusAlongTimes1000 = 5400;
    data.weatherSystemState.activeFront.radiusAcrossTimes1000 = 11200;
    data.weatherSystemState.activeFront.shapeSeed = 101u;
    data.weatherSystemState.activeFront.densitySeed = 202u;
    data.xpSystemState.rngCounter = 17u;
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
    data.kingdoms[0].hasSpawnedBishop = true;
    data.kingdoms[0].lastBishopSpawnParity = 1;
    data.kingdoms[0].pieces.push_back(Piece(0, PieceType::King, KingdomId::White, {0, 0}));
    Building ownedBarracks = makeTestBarracks(10, KingdomId::White, {1, 1}, GameConfig{});
    ownedBarracks.rotationQuarterTurns = 1;
    ownedBarracks.flipMask = 0;
    ownedBarracks.setConstructionState(BuildingState::UnderConstruction);
    ownedBarracks.destroyCellAt(0, 0);
    data.kingdoms[0].buildings.push_back(ownedBarracks);
    Building breachedWall = makeTestStoneWall(11, KingdomId::White, {6, 1}, GameConfig{});
    breachedWall.setCellHP(0, 0, std::max(1, breachedWall.getCellHP(0, 0) - 1));
    breachedWall.setCellBreached(0, 0, true);
    data.kingdoms[0].buildings.push_back(breachedWall);
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
    expect(loaded.xpSystemState.rngCounter == data.xpSystemState.rngCounter,
        "XP RNG state should round-trip through SaveManager.");
    expect(loaded.weatherSystemState.nextSpawnTurnStep == data.weatherSystemState.nextSpawnTurnStep
        && loaded.weatherSystemState.hasActiveFront == data.weatherSystemState.hasActiveFront
        && loaded.weatherSystemState.rngCounter == data.weatherSystemState.rngCounter,
        "Weather scheduler state should round-trip through SaveManager.");
    expect(loaded.weatherSystemState.activeFront.direction == data.weatherSystemState.activeFront.direction
        && loaded.weatherSystemState.activeFront.currentTurnStep == data.weatherSystemState.activeFront.currentTurnStep
        && loaded.weatherSystemState.activeFront.totalTurnSteps == data.weatherSystemState.activeFront.totalTurnSteps
        && loaded.weatherSystemState.activeFront.radiusAcrossTimes1000 == data.weatherSystemState.activeFront.radiusAcrossTimes1000,
        "Active weather front geometry should round-trip through SaveManager.");
    expect(loaded.controllers[0] == ControllerType::Human && loaded.controllers[1] == ControllerType::Human,
       "Legacy controller metadata should stay aligned with session metadata.");
    expect(loaded.multiplayer.enabled && loaded.multiplayer.port == data.multiplayer.port,
        "Multiplayer metadata should round-trip through SaveManager.");
    expect(loaded.multiplayer.passwordHash == data.multiplayer.passwordHash,
        "Multiplayer password hash should round-trip through SaveManager.");
    expect(loaded.kingdoms[0].buildings.size() == 2
        && loaded.kingdoms[0].buildings[0].rotationQuarterTurns == ownedBarracks.rotationQuarterTurns,
        "Owned building rotations should round-trip through SaveManager.");
    expect(loaded.kingdoms[0].buildings[0].isUnderConstruction(),
        "Building construction state should round-trip through SaveManager.");
    expect(loaded.kingdoms[0].buildings[0].getCellHP(0, 0) == 0,
        "Destroyed owned building cells should round-trip through SaveManager.");
    expect(loaded.kingdoms[0].buildings[1].isCellBreached(0, 0),
        "Stone wall breach state should round-trip through SaveManager.");
    expect(loaded.kingdoms[0].hasSpawnedBishop
        && loaded.kingdoms[0].lastBishopSpawnParity == data.kingdoms[0].lastBishopSpawnParity,
        "Kingdom bishop spawn memory should round-trip through SaveManager.");
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
            data.weatherSystemState.hasActiveFront = true;
            data.weatherSystemState.revision = 7;
            data.weatherMaskCache.revision = 7;
            data.weatherMaskCache.diameter = 3;
            data.weatherMaskCache.hasActiveFront = true;
            data.weatherMaskCache.alphaByCell = {0, 12, 24, 48, 96, 144, 192, 220, 255};
            data.weatherMaskCache.shadeByCell = {210, 208, 206, 204, 202, 200, 198, 196, 194};
            data.refreshLegacyMetadataFromSession();

            SaveManager manager;
            const std::string serialized = manager.serialize(data);
            expect(!serialized.empty(), "SaveManager should serialize save data to a non-empty string.");

            SaveData loaded;
            expect(manager.deserialize(serialized, loaded), "SaveManager should deserialize save data from a string snapshot.");
            expect(loaded.gameName == data.gameName, "Serialized string snapshots should preserve the game name.");
            expect(loaded.worldSeed == data.worldSeed, "Serialized string snapshots should preserve the world seed.");
            expect(loaded.multiplayer.port == data.multiplayer.port, "Serialized string snapshots should preserve multiplayer metadata.");
            expect(loaded.weatherSystemState.revision == data.weatherSystemState.revision,
                "Serialized string snapshots should preserve the weather revision used to validate authoritative weather masks.");
            expect(loaded.weatherMaskCache.revision == data.weatherMaskCache.revision
                && loaded.weatherMaskCache.alphaByCell == data.weatherMaskCache.alphaByCell
                && loaded.weatherMaskCache.shadeByCell == data.weatherMaskCache.shadeByCell,
                "Serialized string snapshots should preserve the exact authoritative weather mask payload.");
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

            const TurnCommand disbandCommand = makeDisbandCommand(77);

            sf::Packet packet = createPacket(MultiplayerMessageType::TurnSubmission);
            expect(writePacket(packet, MultiplayerTurnSubmission{{command, disbandCommand}}),
                "Protocol should serialize turn submission packets.");

            MultiplayerMessageType type = MultiplayerMessageType::ServerInfoRequest;
            expect(extractMessageType(packet, type), "Protocol should decode packet types.");
            expect(type == MultiplayerMessageType::TurnSubmission,
                "Packet type should remain the submitted multiplayer message type.");

            MultiplayerTurnSubmission submission;
            expect(readPacket(packet, submission), "Protocol should deserialize turn submission packets.");
            expect(submission.commands.size() == 2, "Turn submission packets should preserve command counts.");
            expect(submission.commands[0].buildOrigin == command.buildOrigin,
                "Turn submission packets should preserve command payloads.");
            expect(submission.commands[0].buildRotationQuarterTurns == command.buildRotationQuarterTurns,
                "Turn submission packets should preserve build rotation payloads.");
            expect(submission.commands[0].upgradeTarget == command.upgradeTarget,
                "Turn submission packets should preserve enum payloads.");
            expect(submission.commands[1].type == TurnCommand::Disband && submission.commands[1].pieceId == 77,
                "Turn submission packets should preserve sacrifice commands.");
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
    addPieceToBoard(white, board, 1, PieceType::King, KingdomId::White, {1, 1});
    std::vector<Building> publicBuildings;
    TurnSystem turnSystem;
    turnSystem.setActiveKingdom(KingdomId::White);
    BuildingFactory buildingFactory;

    TurnCommand buildCommand;
    buildCommand.type = TurnCommand::Build;
    buildCommand.buildingType = BuildingType::Barracks;
    buildCommand.buildOrigin = {2, 2};
    expect(!turnSystem.queueCommand(buildCommand, board, white, black, publicBuildings, config, &buildingFactory),
        "Unaffordable build commands should be rejected during planning.");

    EventLog eventLog;
    PieceFactory pieceFactory;
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
            expect(!turnSystem.queueCommand(produceCommand, board, white, black, publicBuildings, config),
                "Unaffordable production commands should be rejected during planning.");

    EventLog eventLog;
    PieceFactory pieceFactory;
    BuildingFactory buildingFactory;
    turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

    expect(white.gold == 0, "Unaffordable production must not change gold.");
    expect(!white.buildings.front().isProducing, "Unaffordable production must not start barracks production.");
}

    void testBuildSystemAllowsPawnAdjacency() {
        GameConfig config;
        Board board;
        board.init(10);

        Kingdom white(KingdomId::White);
        white.gold = config.getBarracksCost();
        addPieceToBoard(white, board, 1, PieceType::King, KingdomId::White, {1, 1});
        addPieceToBoard(white, board, 2, PieceType::Pawn, KingdomId::White, {5, 5});

        expect(BuildSystem::canBuild(BuildingType::Barracks, {6, 5}, board, white, config),
            "Barracks construction should be valid when a pawn is adjacent even if the king is far away.");
        expect(!BuildSystem::canBuild(BuildingType::Barracks, {10, 10}, board, white, config),
            "Build legality should still reject footprints that are not adjacent to any king or pawn builder.");
    }

    void testTurnSystemRepairsDestroyedOwnedCellWithPawnOccupancy() {
        GameConfig config;
        Board board;
        board.init(10);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        addPieceToBoard(white, board, 1, PieceType::King, KingdomId::White, {0, 0});
        addPieceToBoard(white, board, 2, PieceType::Pawn, KingdomId::White, {3, 3});
        addPieceToBoard(black, board, 3, PieceType::King, KingdomId::Black, {9, 9});
        white.gold = config.getRepairCostPerCell(BuildingType::Barracks);

        white.addBuilding(makeTestBarracks(80, KingdomId::White, {3, 3}, config));
        Building& barracks = white.buildings.back();
        barracks.destroyCellAt(0, 0);
        linkBuildingOnBoard(barracks, board);

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        EventLog eventLog;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;

        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        expect(white.buildings.front().getCellHP(0, 0) == config.getBarracksCellHP(),
            "A pawn standing on a destroyed owned cell should repair it during commit.");
        expect(white.gold == 0,
            "Repairing a destroyed owned cell should deduct the configured per-cell repair cost.");
    }

    void testTurnSystemRepairsBeforeIncome() {
        GameConfig config;
        Board board;
        board.init(10);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        addPieceToBoard(white, board, 1, PieceType::King, KingdomId::White, {0, 0});
        addPieceToBoard(white, board, 2, PieceType::Pawn, KingdomId::White, {3, 3});
        addPieceToBoard(white, board, 3, PieceType::Pawn, KingdomId::White, {7, 7});
        addPieceToBoard(black, board, 4, PieceType::King, KingdomId::Black, {9, 9});
        white.gold = config.getRepairCostPerCell(BuildingType::Barracks) - 1;

        white.addBuilding(makeTestBarracks(81, KingdomId::White, {3, 3}, config));
        Building& barracks = white.buildings.back();
        barracks.destroyCellAt(0, 0);
        linkBuildingOnBoard(barracks, board);

        std::vector<Building> publicBuildings;
        publicBuildings.push_back(makeTestPublicBuilding(BuildingType::Mine, {7, 7}, 1, 1));
        linkBuildingOnBoard(publicBuildings.back(), board);

        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        EventLog eventLog;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;

        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        expect(white.buildings.front().getCellHP(0, 0) == 0,
            "Repairs must resolve before income, so post-income gold cannot repair a cell in the same commit.");
        expect(white.gold == config.getRepairCostPerCell(BuildingType::Barracks) - 1
                    + config.getMineIncomePerCellPerTurn()
                    - (2 * config.getPieceUpkeepCost(PieceType::Pawn)),
            "Income should still be collected after an unaffordable repair attempt is skipped, net of upkeep.");
    }

    void testStoneWallDestroysWhenEnemyStaysOnBreachedCellUntilNextCommit() {
        GameConfig config;
        Board board;
        board.init(10);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        addPieceToBoard(white, board, 1, PieceType::King, KingdomId::White, {0, 0});
        addPieceToBoard(white, board, 2, PieceType::Rook, KingdomId::White, {3, 4});
        addPieceToBoard(black, board, 3, PieceType::King, KingdomId::Black, {9, 9});
        black.addBuilding(makeTestStoneWall(82, KingdomId::Black, {4, 4}, config));
        linkBuildingOnBoard(black.buildings.back(), board);

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        EventLog eventLog;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;

        TurnCommand moveCommand;
        moveCommand.type = TurnCommand::Move;
        moveCommand.pieceId = 2;
        moveCommand.origin = {3, 4};
        moveCommand.destination = {4, 4};
         expect(turnSystem.queueCommand(moveCommand, board, white, black, publicBuildings, config),
             "Initial breach move should queue successfully.");
        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        expect(black.buildings.size() == 1 && black.buildings.front().isCellBreached(0, 0),
            "The first enemy occupancy of a stone wall should leave the wall in a persistent breached state.");
        expect(board.getCell(4, 4).building != nullptr,
            "A breached stone wall should remain on the board after the first commit.");

        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        expect(black.buildings.empty(),
            "A stone wall should be fully destroyed when an enemy piece stays on the breached cell until the next commit.");
        expect(board.getCell(4, 4).building == nullptr,
            "Destroyed stone walls should be removed from the board when the breach is finished.");
    }

    void testStoneWallBreachPersistsAfterAttackerLeavesAndFinishesOnReturn() {
        GameConfig config;
        Board board;
        board.init(10);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        addPieceToBoard(white, board, 1, PieceType::King, KingdomId::White, {0, 0});
        addPieceToBoard(white, board, 2, PieceType::Rook, KingdomId::White, {3, 4});
        addPieceToBoard(black, board, 3, PieceType::King, KingdomId::Black, {9, 9});
        black.addBuilding(makeTestStoneWall(83, KingdomId::Black, {4, 4}, config));
        linkBuildingOnBoard(black.buildings.back(), board);

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        EventLog eventLog;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;

        TurnCommand breachMove;
        breachMove.type = TurnCommand::Move;
        breachMove.pieceId = 2;
        breachMove.origin = {3, 4};
        breachMove.destination = {4, 4};
         expect(turnSystem.queueCommand(breachMove, board, white, black, publicBuildings, config),
             "Breach move should queue successfully.");
        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        TurnCommand leaveMove;
        leaveMove.type = TurnCommand::Move;
        leaveMove.pieceId = 2;
        leaveMove.origin = {4, 4};
        leaveMove.destination = {5, 4};
         expect(turnSystem.queueCommand(leaveMove, board, white, black, publicBuildings, config),
             "Leaving a breached wall should still be allowed.");
        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        expect(black.buildings.size() == 1 && black.buildings.front().isCellBreached(0, 0),
            "A breached stone wall should stay breached after the attacker leaves the cell.");
        expect(!board.isTraversable(4, 4, KingdomId::White),
            "A breached stone wall should still block movement until a later occupancy finishes the destruction.");

        TurnCommand returnMove;
        returnMove.type = TurnCommand::Move;
        returnMove.pieceId = 2;
        returnMove.origin = {5, 4};
        returnMove.destination = {4, 4};
         expect(turnSystem.queueCommand(returnMove, board, white, black, publicBuildings, config),
             "Returning to a breached wall should queue successfully.");
        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        expect(black.buildings.empty(),
            "A later occupancy on a breached stone wall should finish destroying it.");
        expect(board.getCell(4, 4).building == nullptr,
            "The board should clear the wall pointer once a breached stone wall is fully destroyed.");
    }

void testFirstBishopSpawnUsesDefaultNearestRule() {
    GameConfig config;
    Board board;
    board.init(10);

    Kingdom white(KingdomId::White);
    Building barracks = makeTestBarracks(70, KingdomId::White, {7, 8}, config);

    const sf::Vector2i bishopSpawn = ProductionSystem::findSpawnCell(barracks, board, PieceType::Bishop, white);
    const sf::Vector2i pawnSpawn = ProductionSystem::findSpawnCell(barracks, board, PieceType::Pawn, white);
    expect(bishopSpawn == pawnSpawn,
           "The first bishop spawn for a kingdom should use the default nearest-cell spawn rule.");
}

void testBishopSpawnAlternatesAcrossKingdomBarracks() {
    GameConfig config;
    Board board;
    board.init(10);

    Kingdom white(KingdomId::White);
    Building firstBarracks = makeTestBarracks(71, KingdomId::White, {7, 8}, config);
    Building secondBarracks = makeTestBarracks(72, KingdomId::White, {11, 8}, config);

    const sf::Vector2i firstSpawn = ProductionSystem::findSpawnCell(firstBarracks, board, PieceType::Bishop, white);
    white.recordSuccessfulBishopSpawnParity(ProductionSpawnRules::squareColorParity(firstSpawn));

    const sf::Vector2i secondSpawn = ProductionSystem::findSpawnCell(secondBarracks, board, PieceType::Bishop, white);
    expect(ProductionSpawnRules::squareColorParity(secondSpawn)
            != ProductionSpawnRules::squareColorParity(firstSpawn),
           "Bishop spawn parity should alternate across all barracks of a kingdom.");
}

void testBishopSpawnFallsBackWhenPreferredParityUnavailable() {
    GameConfig config;
    Board board;
    board.init(10);

    Kingdom white(KingdomId::White);
    white.recordSuccessfulBishopSpawnParity(0);

    Building barracks = makeTestBarracks(73, KingdomId::White, {7, 8}, config);
    linkBuildingOnBoard(barracks, board);

    for (const sf::Vector2i& cellPos : board.getAllValidCells()) {
        if (barracks.containsCell(cellPos.x, cellPos.y)) {
            continue;
        }

        if (ProductionSpawnRules::squareColorParity(cellPos) == 1) {
            board.getCell(cellPos.x, cellPos.y).type = CellType::Water;
        }
    }

    const sf::Vector2i bishopSpawn = ProductionSystem::findSpawnCell(barracks, board, PieceType::Bishop, white);
    const sf::Vector2i pawnSpawn = ProductionSystem::findSpawnCell(barracks, board, PieceType::Pawn, white);
    expect(bishopSpawn == pawnSpawn,
           "Bishop spawns should fall back to the nearest valid opposite-parity square when the preferred parity is impossible.");
    expect(ProductionSpawnRules::squareColorParity(bishopSpawn) == 0,
           "Fallback bishop spawns should use the available opposite parity when the preferred parity is blocked everywhere.");
}

void testSpawnSearchExpandsBeyondInitialRadius() {
    GameConfig config;
    Board board;
    board.init(10);

    Kingdom white(KingdomId::White);
    Building barracks = makeTestBarracks(74, KingdomId::White, {7, 8}, config);
    linkBuildingOnBoard(barracks, board);

    const std::vector<sf::Vector2i> candidates = ProductionSpawnRules::buildSpawnCandidateOrder(
        barracks.origin,
        barracks.getFootprintWidth(),
        barracks.getFootprintHeight(),
        board.getDiameter());

    for (const sf::Vector2i& candidate : candidates) {
        if (!board.getCell(candidate.x, candidate.y).isInCircle || barracks.containsCell(candidate.x, candidate.y)) {
            continue;
        }

        if (ringDistanceFromBuilding(barracks, candidate) <= 2) {
            board.getCell(candidate.x, candidate.y).type = CellType::Water;
        }
    }

    sf::Vector2i expected{-1, -1};
    for (const sf::Vector2i& candidate : candidates) {
        const Cell& cell = board.getCell(candidate.x, candidate.y);
        if (!cell.isInCircle || cell.type == CellType::Water || cell.building || barracks.containsCell(candidate.x, candidate.y)) {
            continue;
        }

        expected = candidate;
        break;
    }

    expect(expected.x >= 0,
           "The spawn expansion test should leave at least one valid candidate beyond the initial radius.");

    const sf::Vector2i spawn = ProductionSystem::findSpawnCell(barracks, board, PieceType::Pawn, white);
    expect(spawn == expected,
           "Barracks spawn search should expand beyond the previous radius-2 limit when closer rings are blocked.");
}

void testBlockedBishopSpawnKeepsKingdomMemoryUnchanged() {
    GameConfig config;
    Board board;
    board.init(10);

    Kingdom white(KingdomId::White);
    Kingdom black(KingdomId::Black);
    white.recordSuccessfulBishopSpawnParity(1);
    white.addBuilding(makeTestBarracks(75, KingdomId::White, {7, 8}, config));
    Building& barracks = white.buildings.back();
    barracks.isProducing = true;
    barracks.producingType = static_cast<int>(PieceType::Bishop);
    barracks.turnsRemaining = 1;
    linkBuildingOnBoard(barracks, board);

    for (const sf::Vector2i& cellPos : board.getAllValidCells()) {
        if (!barracks.containsCell(cellPos.x, cellPos.y)) {
            board.getCell(cellPos.x, cellPos.y).type = CellType::Water;
        }
    }

    std::vector<Building> publicBuildings;
    TurnSystem turnSystem;
    turnSystem.setActiveKingdom(KingdomId::White);
    EventLog eventLog;
    PieceFactory pieceFactory;
    BuildingFactory buildingFactory;
    turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

    expect(white.pieces.empty(), "Blocked bishop production should not spawn a piece.");
    expect(white.hasSpawnedBishop && white.lastBishopSpawnParity == 1,
           "A failed bishop spawn must not overwrite the kingdom's remembered bishop parity.");
    expect(white.buildings.front().isProducing,
           "Blocked bishop production should stay active so the barracks can retry later.");
}

void testSimultaneousBishopSpawnsAlternateWithinSameTurn() {
    GameConfig config;
    Board board;
    board.init(10);

    Kingdom white(KingdomId::White);
    Kingdom black(KingdomId::Black);
    white.addBuilding(makeTestBarracks(76, KingdomId::White, {7, 8}, config));
    white.addBuilding(makeTestBarracks(77, KingdomId::White, {11, 8}, config));
    for (Building& barracks : white.buildings) {
        barracks.isProducing = true;
        barracks.producingType = static_cast<int>(PieceType::Bishop);
        barracks.turnsRemaining = 1;
    }
    for (Building& barracks : white.buildings) {
        linkBuildingOnBoard(barracks, board);
    }

    std::vector<Building> publicBuildings;
    TurnSystem turnSystem;
    turnSystem.setActiveKingdom(KingdomId::White);
    EventLog eventLog;
    PieceFactory pieceFactory;
    BuildingFactory buildingFactory;
    turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

    expect(white.pieces.size() == 2, "Two completed bishop productions should spawn two bishops when space exists.");
    const int firstParity = ProductionSpawnRules::squareColorParity(white.pieces[0].position);
    const int secondParity = ProductionSpawnRules::squareColorParity(white.pieces[1].position);
    expect(firstParity != secondParity,
           "Multiple bishop spawns in the same turn should alternate using the existing barracks iteration order.");
    expect(white.lastBishopSpawnParity == secondParity,
           "The kingdom should remember the parity of the last bishop that actually spawned this turn.");
}

void testForwardModelMatchesRuntimeBishopSpawnRule() {
    GameConfig config;
    Board board;
    board.init(10);

    Kingdom white(KingdomId::White);
    Kingdom black(KingdomId::Black);
    white.recordSuccessfulBishopSpawnParity(0);
    white.addBuilding(makeTestBarracks(78, KingdomId::White, {7, 8}, config));
    Building& barracks = white.buildings.back();
    barracks.isProducing = true;
    barracks.producingType = static_cast<int>(PieceType::Bishop);
    barracks.turnsRemaining = 1;
    linkBuildingOnBoard(barracks, board);

    const sf::Vector2i runtimeSpawn = ProductionSystem::findSpawnCell(barracks, board, PieceType::Bishop, white);
    GameSnapshot snapshot = ForwardModel::createSnapshot(board, white, black, {}, 1);
    ForwardModel::advanceTurn(snapshot,
                              KingdomId::White,
                              config.getMineIncomePerCellPerTurn(),
                              config.getFarmIncomePerCellPerTurn(),
                              config);

    expect(snapshot.white.pieces.size() == 1,
           "ForwardModel should spawn a produced bishop when runtime conditions allow it.");
    expect(snapshot.white.pieces.front().position == runtimeSpawn,
           "ForwardModel bishop spawns should match the authoritative runtime spawn selection.");
    expect(snapshot.white.lastBishopSpawnParity == ProductionSpawnRules::squareColorParity(runtimeSpawn),
           "ForwardModel should update kingdom bishop parity memory from the square actually used.");
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
            expect(!turnSystem.queueCommand(upgradeCommand, board, white, black, publicBuildings, config),
                "Unaffordable upgrade commands should be rejected during planning.");

    EventLog eventLog;
    PieceFactory pieceFactory;
    BuildingFactory buildingFactory;
    turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

    expect(white.gold == 0, "Unaffordable upgrades must not change gold.");
    expect(white.pieces.front().type == PieceType::Pawn,
           "Unaffordable upgrades must not change the piece type.");
}

    void testTurnSystemCancelsQueuedProductionPerBarracks() {
        GameConfig config;
        Board board;
        board.init(6);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getRecruitCost(PieceType::Knight) + 5;
        white.addBuilding(makeTestBarracks(17, KingdomId::White, {1, 1}, config));
        linkBuildingOnBoard(white.buildings.back(), board);

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);

        TurnCommand produceCommand;
        produceCommand.type = TurnCommand::Produce;
        produceCommand.barracksId = 17;
        produceCommand.produceType = PieceType::Knight;

        expect(turnSystem.queueCommand(produceCommand, board, white, black, publicBuildings, config),
            "Queued production should be accepted for a ready barracks with enough gold.");
        expect(turnSystem.getPendingProduceCommand(17) != nullptr,
            "The queued production should be queryable by barracks id.");
        expect(turnSystem.cancelProduceCommand(17, board, white, black, publicBuildings, config),
            "Queued production should be cancellable per barracks.");
        expect(turnSystem.getPendingProduceCommand(17) == nullptr,
            "Cancelling a queued production should remove it from the pending command list.");
    }

    void testTurnSystemCancelsQueuedUpgradePerPiece() {
        GameConfig config;
        Board board;
        board.init(6);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        Piece& pawn = addPieceToBoard(white, board, 18, PieceType::Pawn, KingdomId::White, {2, 2});
        pawn.xp = config.getXPThresholdPawnToKnightOrBishop();
        white.gold = config.getUpgradeCost(PieceType::Pawn, PieceType::Knight) + 5;

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);

        TurnCommand upgradeCommand;
        upgradeCommand.type = TurnCommand::Upgrade;
        upgradeCommand.upgradePieceId = pawn.id;
        upgradeCommand.upgradeTarget = PieceType::Knight;

        expect(turnSystem.queueCommand(upgradeCommand, board, white, black, publicBuildings, config),
            "Queued upgrades should be accepted for eligible pieces.");
        expect(turnSystem.getPendingUpgradeCommand(pawn.id) != nullptr,
            "Queued upgrades should be queryable by piece id.");
        expect(turnSystem.cancelUpgradeCommand(pawn.id, board, white, black, publicBuildings, config),
            "Queued upgrades should be cancellable per piece.");
        expect(turnSystem.getPendingUpgradeCommand(pawn.id) == nullptr,
            "Cancelling a queued upgrade should remove it from the pending command list.");
    }

    void testSelectionMoveRulesKeepMovesWhenPendingBuildDependsOnCurrentMove() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getWoodWallCost() + 10;
        Piece& whiteKing = addPieceToBoard(white, board, 610, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(black, board, 710, PieceType::King, KingdomId::Black, {18, 18});

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        BuildingFactory buildingFactory;

        const TurnCommand moveCommand = makeMoveCommand(whiteKing.id, {10, 10}, {11, 10});
        const TurnCommand buildCommand = makeBuildCommand(BuildingType::WoodWall, {12, 10});

        expect(turnSystem.queueCommand(moveCommand, board, white, black, publicBuildings, config),
            "The builder move should queue successfully for the reselection regression test.");
        expect(turnSystem.queueCommand(buildCommand, board, white, black, publicBuildings, config, &buildingFactory),
            "The dependent build should queue successfully after the builder move.");

        whiteKing.position = moveCommand.destination;

        const SelectionMoveOptions moveOptions = SelectionMoveRules::classifyPieceMoves(
            board,
            white,
            black,
            publicBuildings,
            1,
            turnSystem.getPendingCommands(),
            whiteKing.id,
            config);

        expect(containsCell(moveOptions.safeMoves, {9, 10}),
            "Reselecting a moved builder should still expose normal move options even if a pending build currently depends on that move.");
    }

    void testTurnSystemReplaceMoveDropsOrphanedPendingBuilds() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getWoodWallCost() + 10;
        Piece& whiteKing = addPieceToBoard(white, board, 611, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(black, board, 711, PieceType::King, KingdomId::Black, {18, 18});

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        BuildingFactory buildingFactory;

        expect(turnSystem.queueCommand(makeMoveCommand(whiteKing.id, {10, 10}, {11, 10}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config),
            "The initial builder move should queue successfully.");
        expect(turnSystem.queueCommand(makeBuildCommand(BuildingType::WoodWall, {12, 10}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config,
                                       &buildingFactory),
            "The dependent build should queue successfully before replacing the move.");

        const int queuedBuildId = turnSystem.getPendingCommands().back().buildId;

        expect(turnSystem.replaceMoveCommand(makeMoveCommand(whiteKing.id, {10, 10}, {9, 10}),
                                             board,
                                             white,
                                             black,
                                             publicBuildings,
                                             config),
            "Replacing a queued move should succeed when the replacement is legal.");

        const TurnCommand* updatedMove = turnSystem.getPendingMoveCommand(whiteKing.id);
        expect(updatedMove != nullptr && updatedMove->destination == sf::Vector2i(9, 10),
            "Replacing the queued move should keep the new destination in the pending command list.");
        expect(turnSystem.getPendingBuildCommand(queuedBuildId) == nullptr,
            "Moving the only supporting builder away should automatically remove the orphaned pending build.");
    }

    void testTurnSystemQueueMoveDropsPendingBuildUnsupportedInFinalState() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getWoodWallCost() + 10;
        Piece& whiteKing = addPieceToBoard(white, board, 6110, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(black, board, 7110, PieceType::King, KingdomId::Black, {18, 18});

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        BuildingFactory buildingFactory;

        expect(turnSystem.queueCommand(makeBuildCommand(BuildingType::WoodWall, {11, 10}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config,
                                       &buildingFactory),
            "The pending build should queue successfully from the builder's current position.");

        const int queuedBuildId = turnSystem.getPendingCommands().front().buildId;

        expect(turnSystem.queueCommand(makeMoveCommand(whiteKing.id, {10, 10}, {9, 10}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config),
            "Queuing a follow-up move should still succeed when the move itself is legal.");

        const TurnCommand* queuedMove = turnSystem.getPendingMoveCommand(whiteKing.id);
        expect(queuedMove != nullptr && queuedMove->destination == sf::Vector2i(9, 10),
            "The queued move should remain in the pending command list.");
        expect(turnSystem.getPendingBuildCommand(queuedBuildId) == nullptr,
            "A pending build should be dropped immediately when the builder's final projected position no longer covers it.");
    }

    void testTurnSystemQueueMoveKeepsPendingBuildWhenAnotherFinalBuilderSupportsIt() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getWoodWallCost() + 10;
        Piece& whiteKing = addPieceToBoard(white, board, 6111, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(white, board, 6112, PieceType::Pawn, KingdomId::White, {12, 11});
        addPieceToBoard(black, board, 7111, PieceType::King, KingdomId::Black, {18, 18});

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        BuildingFactory buildingFactory;

        expect(turnSystem.queueCommand(makeBuildCommand(BuildingType::WoodWall, {11, 10}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config,
                                       &buildingFactory),
            "The pending build should queue successfully while the original builder is adjacent.");

        const int queuedBuildId = turnSystem.getPendingCommands().front().buildId;

        expect(turnSystem.queueCommand(makeMoveCommand(whiteKing.id, {10, 10}, {9, 10}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config),
            "Queuing the builder's follow-up move should succeed when another builder still covers the build in the final state.");
        expect(turnSystem.getPendingBuildCommand(queuedBuildId) != nullptr,
            "A pending build should remain queued when another builder still covers it in the final projected state.");
    }

    void testBuildOverlayRulesExposeOccupiedCellsForMultiCellStructure() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getBarracksCost() + 10;
        addPieceToBoard(white, board, 6113, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(black, board, 7113, PieceType::King, KingdomId::Black, {18, 18});

        const std::vector<sf::Vector2i> buildableOrigins = BuildOverlayRules::collectBuildableOrigins(
            board,
            white,
            black,
            {},
            1,
            {},
            BuildingType::Barracks,
            0,
            config);
        const std::vector<sf::Vector2i> coverageCells = BuildOverlayRules::collectBuildableCoverageCells(
            board,
            white,
            black,
            {},
            1,
            {},
            BuildingType::Barracks,
            0,
            config);

        expect(!buildableOrigins.empty(),
            "The barracks coverage regression test needs at least one valid anchor.");
        expect(coverageCells.size() > buildableOrigins.size(),
            "A multi-cell build overlay should cover more cells than the set of valid anchors.");

        bool foundOccupiedNonAnchorCell = false;
        for (const sf::Vector2i& anchor : buildableOrigins) {
            for (int localY = 0; localY < config.getBuildingHeight(BuildingType::Barracks); ++localY) {
                for (int localX = 0; localX < config.getBuildingWidth(BuildingType::Barracks); ++localX) {
                    const sf::Vector2i occupiedCell(anchor.x + localX, anchor.y + localY);
                    if (!containsCell(buildableOrigins, occupiedCell) && containsCell(coverageCells, occupiedCell)) {
                        foundOccupiedNonAnchorCell = true;
                        break;
                    }
                }

                if (foundOccupiedNonAnchorCell) {
                    break;
                }
            }

            if (foundOccupiedNonAnchorCell) {
                break;
            }
        }

        expect(foundOccupiedNonAnchorCell,
            "The build coverage map should include occupied footprint cells that are not themselves clickable anchors.");
    }

    void testBuildOverlayRulesCoverageFollowsQueuedMoveProjectedState() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getBarracksCost() + 10;
        Piece& whiteKing = addPieceToBoard(white, board, 6114, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(black, board, 7114, PieceType::King, KingdomId::Black, {18, 18});

        const std::vector<TurnCommand> pendingCommands{
            makeMoveCommand(whiteKing.id, {10, 10}, {11, 10})
        };
        const std::vector<sf::Vector2i> coverageBeforeMove = BuildOverlayRules::collectBuildableCoverageCells(
            board,
            white,
            black,
            {},
            1,
            {},
            BuildingType::Barracks,
            0,
            config);
        const std::vector<sf::Vector2i> buildableOrigins = BuildOverlayRules::collectBuildableOrigins(
            board,
            white,
            black,
            {},
            1,
            pendingCommands,
            BuildingType::Barracks,
            0,
            config);
        const std::vector<sf::Vector2i> coverageCells = BuildOverlayRules::collectBuildableCoverageCells(
            board,
            white,
            black,
            {},
            1,
            pendingCommands,
            BuildingType::Barracks,
            0,
            config);

        expect(containsCell(buildableOrigins, {12, 10}),
            "The projected barracks anchors should move with the queued builder move.");
        expect(containsCell(coverageCells, {15, 12}),
            "The build coverage map should include the far edge of a valid projected barracks footprint.");

        bool foundDroppedCell = false;
        for (const sf::Vector2i& cell : coverageBeforeMove) {
            if (!containsCell(coverageCells, cell)) {
                foundDroppedCell = true;
                break;
            }
        }

        bool foundGainedCell = false;
        for (const sf::Vector2i& cell : coverageCells) {
            if (!containsCell(coverageBeforeMove, cell)) {
                foundGainedCell = true;
                break;
            }
        }

        expect(foundDroppedCell,
            "The build coverage map should drop at least one cell when the projected builder position changes.");
        expect(foundGainedCell,
            "The build coverage map should gain at least one new cell when the projected builder position changes.");
    }

    void testStructurePlacementProfilesUseConfiguredAnchorSourceLocals() {
        GameConfig config;

        expectVec2i(StructurePlacementProfiles::getAnchorSourceLocal(BuildingType::Barracks, config),
            {1, 1},
            "Barracks anchor source local should use the configured near-center chunk.");
        expectVec2i(StructurePlacementProfiles::getAnchorSourceLocal(BuildingType::Church, config),
            {1, 1},
            "Church anchor source local should use the configured near-center chunk.");
        expectVec2i(StructurePlacementProfiles::getAnchorSourceLocal(BuildingType::Mine, config),
            {2, 2},
            "Mine anchor source local should use the configured near-center chunk.");
        expectVec2i(StructurePlacementProfiles::getAnchorSourceLocal(BuildingType::Farm, config),
            {2, 1},
            "Farm anchor source local should use the configured near-center chunk.");
        expectVec2i(StructurePlacementProfiles::getAnchorSourceLocal(BuildingType::Arena, config),
            {1, 1},
            "Arena anchor source local should use the configured near-center chunk.");
        expectVec2i(StructurePlacementProfiles::getAnchorSourceLocal(BuildingType::WoodWall, config),
            {0, 0},
            "Wood wall anchor source local should remain the only chunk.");
        expectVec2i(StructurePlacementProfiles::getAnchorSourceLocal(BuildingType::StoneWall, config),
            {0, 0},
            "Stone wall anchor source local should remain the only chunk.");
        expectVec2i(StructurePlacementProfiles::getAnchorSourceLocal(BuildingType::Bridge, 1, 1),
            {0, 0},
            "Bridge anchor source local should remain the only chunk while its footprint is 1x1.");
    }

    void testStructurePlacementProfilesRoundTripAnchorConversionAcrossRotations() {
        GameConfig config;
        const sf::Vector2i anchorCell{20, 17};

        for (int rotation = 0; rotation < 4; ++rotation) {
            const sf::Vector2i canonicalOrigin = StructurePlacementProfiles::originFromAnchorCell(
                BuildingType::Farm,
                anchorCell,
                rotation,
                config);
            const sf::Vector2i resolvedAnchorCell = StructurePlacementProfiles::anchorCellFromOrigin(
                BuildingType::Farm,
                canonicalOrigin,
                rotation,
                config);
            expectVec2i(resolvedAnchorCell,
                anchorCell,
                "Anchor conversion should round-trip for all farm rotations.");
        }
    }

    void testBuildOverlayRulesOriginsMatchBruteForceProjection() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getBarracksCost() * 2;
        Piece& whiteKing = addPieceToBoard(white, board, 6115, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(white, board, 6116, PieceType::Pawn, KingdomId::White, {13, 11});
        addPieceToBoard(black, board, 7115, PieceType::King, KingdomId::Black, {18, 18});

        Building blocker = makeTestStoneWall(8115, KingdomId::White, {8, 8}, config);
        white.addBuilding(blocker);
        linkBuildingOnBoard(white.buildings.back(), board);

        const std::vector<TurnCommand> pendingCommands{
            makeMoveCommand(whiteKing.id, {10, 10}, {11, 10})
        };

        const std::vector<sf::Vector2i> optimizedOrigins = BuildOverlayRules::collectBuildableOrigins(
            board,
            white,
            black,
            {},
            1,
            pendingCommands,
            BuildingType::Barracks,
            0,
            config);

        std::vector<sf::Vector2i> bruteForceOrigins;
        for (const sf::Vector2i& origin : board.getAllValidCells()) {
            TurnCommand candidate = makeBuildCommand(BuildingType::Barracks, origin);
            if (PendingTurnProjection::canAppendCommand(
                    board,
                    white,
                    black,
                    {},
                    1,
                    pendingCommands,
                    candidate,
                    config)) {
                bruteForceOrigins.push_back(origin);
            }
        }

        expect(sameCellSet(optimizedOrigins, bruteForceOrigins),
            "Optimized build overlay origins must match brute-force projected build legality exactly.");

        const BuildOverlayRules::BuildOverlayMap overlayMap = BuildOverlayRules::collectBuildOverlayMap(
            board,
            white,
            black,
            {},
            1,
            pendingCommands,
            BuildingType::Barracks,
            0,
            config);
        std::vector<sf::Vector2i> anchorConvertedOrigins;
        anchorConvertedOrigins.reserve(overlayMap.validAnchorCells.size());
        for (const sf::Vector2i& anchorCell : overlayMap.validAnchorCells) {
            anchorConvertedOrigins.push_back(StructurePlacementProfiles::originFromAnchorCell(
                BuildingType::Barracks,
                anchorCell,
                0,
                config));
        }

        expect(sameCellSet(anchorConvertedOrigins, bruteForceOrigins),
            "Valid anchor cells should convert back to the exact same canonical build origins.");
    }

    void testBuildOverlayCoordinatorRefreshesClearsAndInvalidatesCache() {
        GameConfig config;
        GameEngine engine;
        engine.board().init(12);
        engine.kingdom(KingdomId::White) = Kingdom(KingdomId::White);
        engine.kingdom(KingdomId::Black) = Kingdom(KingdomId::Black);
        engine.publicBuildings().clear();
        engine.turnSystem().setActiveKingdom(KingdomId::White);
        engine.turnSystem().setTurnNumber(1);
        engine.kingdom(KingdomId::White).gold = config.getWoodWallCost() + 10;

        Piece& whiteKing = addPieceToBoard(engine.kingdom(KingdomId::White),
                                           engine.board(),
                                           6117,
                                           PieceType::King,
                                           KingdomId::White,
                                           {10, 10});
        addPieceToBoard(engine.kingdom(KingdomId::Black),
                        engine.board(),
                        7117,
                        PieceType::King,
                        KingdomId::Black,
                        {18, 18});

        std::string error;
        expect(engine.replacePendingCommands({makeMoveCommand(whiteKing.id, {10, 10}, {11, 10})}, config, false, &error),
            error);

        const TurnValidationContext turnContext = engine.makeTurnValidationContext(config);
        BuildOverlayRuntimeState runtimeState;
        runtimeState.activeTool = ToolState::Build;
        runtimeState.permissions.canShowBuildPreview = true;
        runtimeState.permissions.canQueueNonMoveActions = true;
        runtimeState.revision = engine.turnSystem().getPendingStateRevision();
        runtimeState.turnNumber = engine.turnSystem().getTurnNumber();
        runtimeState.activeKingdom = engine.activeKingdom().id;
        runtimeState.buildType = BuildingType::WoodWall;
        runtimeState.rotationQuarterTurns = 0;
        runtimeState.canQueueNonMoveActions = true;

        const BuildOverlayRules::BuildOverlayMap expectedMap = BuildOverlayRules::collectBuildOverlayMap(
            turnContext,
            engine.turnSystem().getPendingCommands(),
            runtimeState.buildType,
            runtimeState.rotationQuarterTurns);
        expect(!expectedMap.validAnchorCells.empty() && !expectedMap.coverageCells.empty(),
            "BuildOverlayCoordinator test should use a projected state that exposes a non-empty build preview.");

        BuildOverlayCache cache;
        BuildOverlayCoordinator::refresh(runtimeState,
                                         turnContext,
                                         engine.turnSystem().getPendingCommands(),
                                         cache);
        expect(cache.cacheValid,
            "BuildOverlayCoordinator should mark the cache as valid after computing build preview overlays.");
        expect(sameCellSet(cache.validAnchorCells, expectedMap.validAnchorCells),
            "BuildOverlayCoordinator should reuse BuildOverlayRules exactly for valid anchor cells.");
        expect(sameCellSet(cache.coverageCells, expectedMap.coverageCells),
            "BuildOverlayCoordinator should reuse BuildOverlayRules exactly for coverage cells.");

        runtimeState.permissions.canQueueNonMoveActions = false;
        runtimeState.canQueueNonMoveActions = false;
        BuildOverlayCoordinator::refresh(runtimeState,
                                         turnContext,
                                         engine.turnSystem().getPendingCommands(),
                                         cache);
        expect(cache.cacheValid,
            "BuildOverlayCoordinator should keep a valid cache key after clearing overlays because non-move actions are temporarily disallowed.");
        expect(cache.validAnchorCells.empty() && cache.coverageCells.empty(),
            "BuildOverlayCoordinator should clear projected build overlays when build actions are no longer queueable for the current local state.");

        runtimeState.activeTool = ToolState::Select;
        BuildOverlayCoordinator::refresh(runtimeState,
                                         turnContext,
                                         engine.turnSystem().getPendingCommands(),
                                         cache);
        expect(!cache.cacheValid,
            "BuildOverlayCoordinator should invalidate the cached build preview when the active tool stops being the build tool.");
    }

    void testTurnSystemCancelMoveKeepsBuildWhenAnotherBuilderStillSupportsIt() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getWoodWallCost() + 10;
        Piece& whiteKing = addPieceToBoard(white, board, 612, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(white, board, 613, PieceType::Pawn, KingdomId::White, {12, 11});
        addPieceToBoard(black, board, 712, PieceType::King, KingdomId::Black, {18, 18});

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        BuildingFactory buildingFactory;

        expect(turnSystem.queueCommand(makeMoveCommand(whiteKing.id, {10, 10}, {11, 10}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config),
            "The initial move should queue successfully for the shared-builder regression test.");
        expect(turnSystem.queueCommand(makeBuildCommand(BuildingType::WoodWall, {12, 10}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config,
                                       &buildingFactory),
            "The build should queue successfully while multiple builders can support it.");

        const int queuedBuildId = turnSystem.getPendingCommands().back().buildId;

        expect(turnSystem.cancelMoveCommand(whiteKing.id, board, white, black, publicBuildings, config),
            "Cancelling the queued move should succeed.");
        expect(turnSystem.getPendingMoveCommand(whiteKing.id) == nullptr,
            "Cancelling the queued move should remove the move command itself.");
        expect(turnSystem.getPendingBuildCommand(queuedBuildId) != nullptr,
            "A pending build should remain queued when another adjacent builder still supports it after move cancellation.");
    }

    void testTurnSystemAssignsStablePendingBuildIds() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = (config.getWoodWallCost() * 2) + 10;
        addPieceToBoard(white, board, 614, PieceType::King, KingdomId::White, {5, 5});
        addPieceToBoard(black, board, 714, PieceType::King, KingdomId::Black, {9, 9});

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        BuildingFactory buildingFactory;

        expect(turnSystem.queueCommand(makeBuildCommand(BuildingType::WoodWall, {6, 5}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config,
                                       &buildingFactory),
            "The first pending build should queue successfully.");
        expect(turnSystem.queueCommand(makeBuildCommand(BuildingType::WoodWall, {5, 6}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config,
                                       &buildingFactory),
            "The second pending build should queue successfully.");

        expect(turnSystem.getPendingCommands().size() == 2,
            "Two queued builds should remain in the pending command list.");

        const int firstBuildId = turnSystem.getPendingCommands()[0].buildId;
        const int secondBuildId = turnSystem.getPendingCommands()[1].buildId;
        expect(firstBuildId >= 100,
            "Queued builds should now reserve stable positive ids from the building factory.");
        expect(secondBuildId > firstBuildId,
            "Later queued builds should reserve later stable ids from the same allocator.");
        expect(firstBuildId != secondBuildId,
            "Queued builds should receive distinct stable ids.");
        expect(turnSystem.getPendingBuildCommand(firstBuildId) != nullptr,
            "Queued builds should be queryable by their stable reserved build id.");
        expect(turnSystem.cancelBuildCommand(firstBuildId, board, white, black, publicBuildings, config),
            "Queued builds should be cancellable by their stable build id.");
        expect(turnSystem.getPendingBuildCommand(firstBuildId) == nullptr,
            "Cancelling a queued build by id should remove that build.");
        expect(turnSystem.getPendingBuildCommand(secondBuildId) != nullptr,
            "Cancelling one queued build should leave the others intact.");
    }

    void testTurnSystemCommitPreservesReservedBuildId() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getWoodWallCost() + 10;
        addPieceToBoard(white, board, 615, PieceType::King, KingdomId::White, {5, 5});
        addPieceToBoard(black, board, 715, PieceType::King, KingdomId::Black, {9, 9});

        std::vector<Building> publicBuildings;
        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        EventLog eventLog;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;

        expect(turnSystem.queueCommand(makeBuildCommand(BuildingType::WoodWall, {6, 5}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config,
                                       &buildingFactory),
            "The queued build should reserve a stable build id before commit.");

        const int reservedBuildId = turnSystem.getPendingCommands().front().buildId;
        turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);

        Building* committedBuilding = findBuildingById(white, reservedBuildId);
        expect(committedBuilding != nullptr,
            "Committing a queued build should materialize the authoritative building with the reserved id.");
        expect(!committedBuilding->isUnderConstruction(),
            "The committed authoritative building should be completed immediately after turn validation.");
    }

    void testTurnDraftMaterializesQueuedBuildingAsUnderConstruction() {
        GameConfig config;
        GameEngine engine;
        engine.board().init(12);
        engine.kingdom(KingdomId::White) = Kingdom(KingdomId::White);
        engine.kingdom(KingdomId::Black) = Kingdom(KingdomId::Black);
        engine.publicBuildings().clear();
        engine.turnSystem().setActiveKingdom(KingdomId::White);
        engine.turnSystem().setTurnNumber(1);
        engine.kingdom(KingdomId::White).gold = config.getBarracksCost() + 10;

        TurnCommand buildCommand = makeBuildCommand(BuildingType::Barracks, {3, 3});
        buildCommand.buildId = 177;

        TurnDraft draft;
        expect(draft.rebuild(engine, config, {buildCommand}),
            "Rebuilding the local turn draft should replay queued build commands.");

        Building* draftedBuilding = findBuildingById(draft.kingdom(KingdomId::White), buildCommand.buildId);
        expect(draftedBuilding != nullptr,
            "Queued build commands should materialize as concrete buildings inside the turn draft.");
        expect(draftedBuilding->isUnderConstruction(),
            "Queued buildings in the turn draft should be flagged as under construction.");
        expect(!draftedBuilding->isUsable(),
            "Under-construction buildings should not be usable before turn validation.");
        expect(!draftedBuilding->hasActiveGameplayEffects(),
            "Under-construction buildings should not activate passive gameplay effects before validation.");
        expect(!ProductionSystem::canStartProduction(*draftedBuilding,
                                                     PieceType::Pawn,
                                                     draft.kingdom(KingdomId::White),
                                                     config),
            "Under-construction barracks should reject production even if they are already materialized in the local draft.");
        expect(draft.board().getCell(buildCommand.buildOrigin.x, buildCommand.buildOrigin.y).building == draftedBuilding,
            "A materialized draft building should immediately occupy its board cells.");
        expect(draft.kingdom(KingdomId::White).gold == engine.kingdom(KingdomId::White).gold - config.getBarracksCost(),
            "Replaying a queued build into the turn draft should reserve the building cost from the displayed gold.");
    }

    void testTurnDraftCapturesAutonomousUnitOnQueuedMove() {
        GameConfig config;
        GameEngine engine;
        engine.board().init(12);
        engine.kingdom(KingdomId::White) = Kingdom(KingdomId::White);
        engine.kingdom(KingdomId::Black) = Kingdom(KingdomId::Black);
        engine.publicBuildings().clear();
        engine.mapObjects().clear();
        engine.autonomousUnits().clear();

        addPieceToBoard(engine.kingdom(KingdomId::White),
                        engine.board(),
                        801,
                        PieceType::Rook,
                        KingdomId::White,
                        {4, 4});
        addPieceToBoard(engine.kingdom(KingdomId::White),
                        engine.board(),
                        802,
                        PieceType::King,
                        KingdomId::White,
                        {1, 1});
        addPieceToBoard(engine.kingdom(KingdomId::Black),
                        engine.board(),
                        901,
                        PieceType::King,
                        KingdomId::Black,
                        {10, 10});

        engine.autonomousUnits().push_back(makeTestInfernalUnit(5001, KingdomId::Black, {4, 7}));
        relinkBoardState(engine.board(),
                         engine.kingdoms(),
                         engine.publicBuildings(),
                         engine.mapObjects(),
                         engine.autonomousUnits());

        TurnDraft draft;
        expect(draft.rebuild(engine, config, {makeMoveCommand(801, {4, 4}, {4, 7})}),
            "TurnDraft should rebuild successfully when a queued move captures an autonomous unit.");
        expect(draft.autonomousUnits().empty(),
            "The projected draft should remove an autonomous unit that is captured by a queued move.");
        expect(draft.board().getCell(4, 7).autonomousUnit == nullptr,
            "The projected destination cell should clear autonomous occupancy after the capture is replayed.");
        expect(draft.board().getCell(4, 7).piece != nullptr
               && draft.board().getCell(4, 7).piece->id == 801,
            "The projected moving piece should occupy the destination cell after capturing the autonomous unit.");
    }

    void testTurnSystemCapturesAutonomousUnitAndClearsInfernalState() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        addPieceToBoard(white, board, 811, PieceType::Rook, KingdomId::White, {4, 4});
        addPieceToBoard(white, board, 812, PieceType::King, KingdomId::White, {1, 1});
        addPieceToBoard(black, board, 911, PieceType::King, KingdomId::Black, {10, 10});

        std::vector<Building> publicBuildings;
        std::vector<MapObject> mapObjects;
        ChestSystemState chestSystemState{};
        std::vector<AutonomousUnit> autonomousUnits;
        autonomousUnits.push_back(makeTestInfernalUnit(5002, KingdomId::Black, {4, 7}));
        board.getCell(4, 7).autonomousUnit = &autonomousUnits.front();

        InfernalSystemState infernalSystemState{};
        infernalSystemState.activeInfernalUnitId = 5002;

        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        turnSystem.setTurnNumber(1);
        EventLog eventLog;
        std::vector<GameplayNotification> gameplayNotifications;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;
        XPSystemState xpSystemState{};
        const std::uint32_t worldSeed = 123456u;

        expect(turnSystem.queueCommand(makeMoveCommand(811, {4, 4}, {4, 7}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config),
            "A kingdom piece should be allowed to queue a move that captures an autonomous unit.");

        turnSystem.commitTurn(board,
                              white,
                              black,
                              publicBuildings,
                              mapObjects,
                              chestSystemState,
                              xpSystemState,
                              autonomousUnits,
                              infernalSystemState,
                              worldSeed,
                              config,
                              eventLog,
                              gameplayNotifications,
                              pieceFactory,
                              buildingFactory);

        expect(autonomousUnits.empty(),
            "Authoritative turn commit should remove an autonomous unit captured by a kingdom piece.");
        expect(infernalSystemState.activeInfernalUnitId == -1,
            "Capturing the active infernal unit should clear the active infernal id from system state.");
        expect(board.getCell(4, 7).autonomousUnit == nullptr,
            "The authoritative destination cell should no longer reference the captured autonomous unit.");
        expect(board.getCell(4, 7).piece != nullptr && board.getCell(4, 7).piece->id == 811,
            "The capturing piece should finish the committed move on the destination cell.");
    }

    void testXPConfigLoadsStructuredProfiles() {
        GameConfig config = makeXPTestConfig(
            "      \"thresholds\": {\n"
            "        \"pawn_to_knight_or_bishop\": 33,\n"
            "        \"to_rook\": 77\n"
            "      },\n"
            "      \"sources\": {\n"
            "        \"kill_pawn\": {\n"
            "          \"mean\": 41,\n"
            "          \"sigma_multiplier_times_100\": 25,\n"
            "          \"clamp_sigma_multiplier_times_100\": 150,\n"
            "          \"minimum\": 9\n"
            "        },\n"
            "        \"destroy_block\": {\n"
            "          \"mean\": 12,\n"
            "          \"sigma_multiplier_times_100\": 5,\n"
            "          \"clamp_sigma_multiplier_times_100\": 50,\n"
            "          \"minimum\": 3\n"
            "        }\n"
            "      }");

        const XPRewardProfile pawnProfile = config.getXPRewardProfile(XPRewardSource::KillPawn);
        const XPRewardProfile blockProfile = config.getXPRewardProfile(XPRewardSource::DestroyBlock);
        expect(config.getXPThresholdPawnToKnightOrBishop() == 33,
            "Structured XP config should override the pawn promotion threshold.");
        expect(config.getXPThresholdToRook() == 77,
            "Structured XP config should override the rook promotion threshold.");
        expect(pawnProfile.mean == 41
            && pawnProfile.sigmaMultiplierTimes100 == 25
            && pawnProfile.clampSigmaMultiplierTimes100 == 150
            && pawnProfile.minimum == 9,
            "Structured XP config should load the full per-source kill-pawn profile.");
        expect(blockProfile.mean == 12
            && blockProfile.sigmaMultiplierTimes100 == 5
            && blockProfile.clampSigmaMultiplierTimes100 == 50
            && blockProfile.minimum == 3,
            "Structured XP config should load the full per-source destroy-block profile.");
        expect(config.getKillXP(PieceType::Pawn) == 41,
            "Legacy XP getter compatibility should expose the configured mean for kill-pawn rewards.");
        expect(config.getDestroyBlockXP() == 12,
            "Legacy destroy-block getter compatibility should expose the configured mean.");
    }

    void testCheatcodeConfigLoadsBooleanAndShortcuts() {
        const std::filesystem::path tempPath =
            std::filesystem::temp_directory_path() / "anormalchess_cheatcode_test_config.json";
        {
            std::ofstream out(tempPath);
            out << "{\n"
                << "  \"game\": {\n"
                << "    \"cheatcode\": {\n"
                << "      \"enabled\": true,\n"
                << "      \"shortcuts\": {\n"
                << "        \"weather_fog\": \"F1\",\n"
                << "        \"chest_loot\": \"F2\",\n"
                << "        \"infernal_piece\": \"F3\"\n"
                << "      }\n"
                << "    }\n"
                << "  }\n"
                << "}\n";
        }

        GameConfig config;
        expect(config.loadFromFile(tempPath.string()),
            "GameConfig should load cheatcode settings from a temporary override file.");
        std::filesystem::remove(tempPath);

        expect(config.isCheatcodeEnabled(),
            "Cheatcode config should enable the feature when the JSON boolean is true.");
        expect(config.getCheatcodeWeatherShortcut() == sf::Keyboard::F1,
            "Cheatcode config should load the configured weather shortcut.");
        expect(config.getCheatcodeChestShortcut() == sf::Keyboard::F2,
            "Cheatcode config should load the configured chest shortcut.");
        expect(config.getCheatcodeInfernalShortcut() == sf::Keyboard::F3,
            "Cheatcode config should load the configured infernal shortcut.");
    }

    void testWeatherConfigLoadsStructuredParameters() {
        GameConfig config = makeWeatherTestConfig(
            "      \"cooldown_min_turns\": 2,\n"
            "      \"arrival_gamma_shape_times_100\": 150,\n"
            "      \"arrival_gamma_scale_times_100\": 125,\n"
            "      \"duration_gamma_shape_times_100\": 310,\n"
            "      \"duration_gamma_scale_times_100\": 90,\n"
            "      \"speed_blocks_per_100_turns\": 37,\n"
            "      \"direction_weights\": {\n"
            "        \"north\": 7,\n"
            "        \"south\": 1,\n"
            "        \"east\": 3,\n"
            "        \"west\": 5,\n"
            "        \"north_east\": 9,\n"
            "        \"north_west\": 11,\n"
            "        \"south_east\": 13,\n"
            "        \"south_west\": 15\n"
            "      },\n"
            "      \"entry_center_weight_times_100\": 220,\n"
            "      \"entry_corner_weight_times_100\": 40,\n"
            "      \"coverage_min_percent\": 28,\n"
            "      \"coverage_max_percent\": 34,\n"
            "      \"aspect_ratio_min_times_100\": 190,\n"
            "      \"aspect_ratio_max_times_100\": 275,\n"
            "      \"shape_noise_cell_span\": 8,\n"
            "      \"shape_noise_amplitude_percent\": 26,\n"
            "      \"edge_softness_percent\": 21,\n"
            "      \"alpha_base_percent\": 55,\n"
            "      \"alpha_min_percent\": 25,\n"
            "      \"alpha_max_percent\": 88,\n"
            "      \"density_mu_times_100\": -20,\n"
            "      \"density_sigma_times_100\": 42");

        const std::array<int, kNumWeatherDirections> weights = config.getWeatherDirectionWeights();
        expect(config.getWeatherCooldownMinTurns() == 2,
            "Structured weather config should override the minimum cooldown between fog fronts.");
        expect(config.getWeatherArrivalGammaShapeTimes100() == 150
            && config.getWeatherArrivalGammaScaleTimes100() == 125,
            "Structured weather config should override the arrival gamma parameters.");
        expect(config.getWeatherDurationGammaShapeTimes100() == 310
            && config.getWeatherDurationGammaScaleTimes100() == 90,
            "Structured weather config should override the duration gamma parameters.");
        expect(config.getWeatherSpeedBlocksPer100Turns() == 37,
            "Structured weather config should load the configured front speed measured in blocks per 100 turns.");
        expect(weights[weatherDirectionIndex(WeatherDirection::North)] == 7
            && weights[weatherDirectionIndex(WeatherDirection::SouthWest)] == 15,
            "Structured weather config should load the full per-direction weight table.");
        expect(config.getWeatherCoverageMinPercent() == 28
            && config.getWeatherCoverageMaxPercent() == 34,
            "Structured weather config should load the configured fog coverage band.");
        expect(config.getWeatherAlphaBasePercent() == 55
            && config.getWeatherAlphaMinPercent() == 25
            && config.getWeatherAlphaMaxPercent() == 88,
            "Structured weather config should load the configured opacity controls.");
        expect(config.getWeatherDensityMuTimes100() == -20
            && config.getWeatherDensitySigmaTimes100() == 42,
            "Structured weather config should load the configured log-normal density parameters.");
    }

    void testWeatherSystemUsesDeterministicSerializedSequence() {
        GameConfig config = makeWeatherTestConfig(
            "      \"cooldown_min_turns\": 0,\n"
            "      \"arrival_gamma_shape_times_100\": 1,\n"
            "      \"arrival_gamma_scale_times_100\": 1,\n"
            "      \"duration_gamma_shape_times_100\": 180,\n"
            "      \"duration_gamma_scale_times_100\": 90,\n"
            "      \"coverage_min_percent\": 30,\n"
            "      \"coverage_max_percent\": 30,\n"
            "      \"aspect_ratio_min_times_100\": 220,\n"
            "      \"aspect_ratio_max_times_100\": 220,\n"
            "      \"alpha_base_percent\": 60,\n"
            "      \"alpha_min_percent\": 30,\n"
            "      \"alpha_max_percent\": 80");

        Board board;
        board.init(12);

        WeatherSystemState firstState{};
        WeatherSystemState replayState{};
        firstState.nextSpawnTurnStep = 0;
        replayState.nextSpawnTurnStep = 0;
        const std::uint32_t worldSeed = 515151u;

        expect(WeatherSystem::trySpawnFront(firstState, board, worldSeed, 0, config),
            "WeatherSystem should spawn a fog front when the serialized schedule says it is due.");
        expect(WeatherSystem::trySpawnFront(replayState, board, worldSeed, 0, config),
            "WeatherSystem replay should spawn the same fog front from the same serialized state.");

        expect(firstState.hasActiveFront && replayState.hasActiveFront,
            "Deterministic weather replay should leave both states with an active front.");
        expect(firstState.activeFront.direction == replayState.activeFront.direction
            && firstState.activeFront.totalTurnSteps == replayState.activeFront.totalTurnSteps
            && firstState.activeFront.centerStartXTimes1000 == replayState.activeFront.centerStartXTimes1000
            && firstState.activeFront.centerStartYTimes1000 == replayState.activeFront.centerStartYTimes1000
            && firstState.activeFront.stepXTimes1000 == replayState.activeFront.stepXTimes1000
            && firstState.activeFront.stepYTimes1000 == replayState.activeFront.stepYTimes1000
            && firstState.activeFront.radiusAlongTimes1000 == replayState.activeFront.radiusAlongTimes1000
            && firstState.activeFront.radiusAcrossTimes1000 == replayState.activeFront.radiusAcrossTimes1000
            && firstState.activeFront.shapeSeed == replayState.activeFront.shapeSeed
            && firstState.activeFront.densitySeed == replayState.activeFront.densitySeed,
            "Weather spawning should be deterministic for the same world seed and serialized RNG state.");

        WeatherMaskCache firstMask;
        WeatherMaskCache replayMask;
        WeatherSystem::rebuildMask(board, firstState, config, firstMask);
        WeatherSystem::rebuildMask(board, replayState, config, replayMask);
        expect(firstMask.alphaByCell == replayMask.alphaByCell,
            "Rebuilding the fog mask from equivalent serialized weather states should yield the same concealment map.");

        WeatherSystem::advanceFront(firstState, worldSeed, 1, config);
        WeatherSystemState resumedState = replayState;
        WeatherSystem::advanceFront(replayState, worldSeed, 1, config);
        WeatherSystem::advanceFront(resumedState, worldSeed, 1, config);
        expect(replayState.activeFront.currentTurnStep == resumedState.activeFront.currentTurnStep,
            "Restoring the serialized weather state should resume the exact same front progression step.");

        WeatherMaskCache resumedMask;
        WeatherSystem::rebuildMask(board, replayState, config, replayMask);
        WeatherSystem::rebuildMask(board, resumedState, config, resumedMask);
        expect(replayMask.alphaByCell == resumedMask.alphaByCell,
            "Restored weather fronts should rebuild the exact same concealment mask after advancing.");
    }

    void testWeatherSystemUsesConfiguredSpeedBlocksPer100Turns() {
        GameConfig config = makeWeatherTestConfig(
            "      \"cooldown_min_turns\": 0,\n"
            "      \"speed_blocks_per_100_turns\": 50,\n"
            "      \"direction_weights\": {\n"
            "        \"north\": 0,\n"
            "        \"south\": 0,\n"
            "        \"east\": 1,\n"
            "        \"west\": 0,\n"
            "        \"north_east\": 0,\n"
            "        \"north_west\": 0,\n"
            "        \"south_east\": 0,\n"
            "        \"south_west\": 0\n"
            "      },\n"
            "      \"coverage_min_percent\": 30,\n"
            "      \"coverage_max_percent\": 30");

        Board board;
        board.init(12);

        WeatherSystemState state{};
        state.nextSpawnTurnStep = 0;
        expect(WeatherSystem::trySpawnFront(state, board, 919191u, 0, config),
            "WeatherSystem should spawn a front for speed validation when the schedule is due.");

        const float stepMagnitude = std::sqrt(
            static_cast<float>(state.activeFront.stepXTimes1000 * state.activeFront.stepXTimes1000
                + state.activeFront.stepYTimes1000 * state.activeFront.stepYTimes1000)) / 1000.0f;
        expect(std::abs(stepMagnitude - 0.25f) <= 0.01f,
            "A speed of 50 blocks per 100 turns should move the front center by about 0.25 blocks per half-turn.");

        const float travelAfterTwoTurns = stepMagnitude * 4.0f;
        expect(std::abs(travelAfterTwoTurns - 1.0f) <= 0.05f,
            "A speed of 50 blocks per 100 turns should move the front center by about 1 block every 2 turns.");
    }

    void testGameEngineCheatcodeTriggersSpawnEvents() {
        GameConfig config;
        GameEngine engine;
        GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "cheatcode_event_test");

        std::string error;
        expect(engine.startNewSession(session, config, &error), error);

        const sf::Vector2i boardCenter{engine.board().getRadius(), engine.board().getRadius()};
        sf::Vector2i infernalTargetCell = findEmptyTraversableCell(engine);
        int bestCenterDistance = std::numeric_limits<int>::max();
        for (const sf::Vector2i& cellPos : engine.board().getAllValidCells()) {
            const Cell& cell = engine.board().getCell(cellPos.x, cellPos.y);
            if (cell.type == CellType::Water || cell.type == CellType::Void) {
                continue;
            }
            if (cell.piece != nullptr || cell.building != nullptr || cell.autonomousUnit != nullptr) {
                continue;
            }

            const int centerDistance = std::abs(cellPos.x - boardCenter.x) + std::abs(cellPos.y - boardCenter.y);
            if (centerDistance < bestCenterDistance) {
                bestCenterDistance = centerDistance;
                infernalTargetCell = cellPos;
            }
        }

        addPieceToBoard(engine.kingdom(KingdomId::White),
                        engine.board(),
                        9100,
                        PieceType::Queen,
                        KingdomId::White,
                        infernalTargetCell);
        relinkBoardState(engine.board(),
                         engine.kingdoms(),
                         engine.publicBuildings(),
                         engine.mapObjects(),
                         engine.autonomousUnits());

        expect(engine.triggerCheatcodeWeatherFront(config),
            "GameEngine should spawn a weather front immediately when the weather cheatcode is triggered.");
        expect(engine.weatherSystemState().hasActiveFront,
            "Triggering the weather cheatcode should leave an active weather front in authoritative state.");

        expect(engine.triggerCheatcodeChestSpawn(config),
            "GameEngine should spawn a chest immediately when the chest cheatcode is triggered and no chest is active.");
        expect(engine.chestSystemState().activeChestObjectId >= 0,
            "Triggering the chest cheatcode should leave an active chest object tracked in authoritative state.");

        expect(engine.triggerCheatcodeInfernalSpawn(config),
            "GameEngine should spawn an infernal unit immediately when the infernal cheatcode is triggered and a valid target exists.");
        expect(engine.infernalSystemState().activeInfernalUnitId >= 0,
            "Triggering the infernal cheatcode should leave an active infernal unit tracked in authoritative state.");
        expect(!engine.autonomousUnits().empty(),
            "Triggering the infernal cheatcode should materialize an autonomous infernal unit on the board.");
    }

    void testInputHandlerReconcileSelectionDemotesFoggedEnemyPieceToTerrain() {
        GameConfig config;
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        addPieceToBoard(white, board, 1300, PieceType::King, KingdomId::White, {1, 1});
        Piece& hiddenEnemyPawn = addPieceToBoard(black,
                                                 board,
                                                 2300,
                                                 PieceType::Pawn,
                                                 KingdomId::Black,
                                                 {5, 5});
        addPieceToBoard(black, board, 2301, PieceType::King, KingdomId::Black, {9, 9});

        WeatherMaskCache weatherMaskCache;
        weatherMaskCache.diameter = board.getDiameter();
        weatherMaskCache.hasActiveFront = true;
        weatherMaskCache.alphaByCell.assign(
            static_cast<std::size_t>(weatherMaskCache.diameter * weatherMaskCache.diameter),
            0);
        weatherMaskCache.alphaByCell[static_cast<std::size_t>((5 * weatherMaskCache.diameter) + 5)] = 255;

        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        turnSystem.setTurnNumber(1);
        BuildingFactory buildingFactory;
        std::vector<Building> publicBuildings;
        sf::RenderWindow window;
        Camera camera;
        UIManager uiManager;
        InputContext context = makePassiveInputContext(window,
                                                       camera,
                                                       board,
                                                       turnSystem,
                                                       buildingFactory,
                                                       white,
                                                       black,
                                                       publicBuildings,
                                                       uiManager,
                                                       config);
        context.weatherMaskCache = &weatherMaskCache;
        context.localPerspectiveKingdom = KingdomId::White;
        context.permissions.canIssueCommands = false;

        InputSelectionBookmark bookmark;
        bookmark.pieceId = hiddenEnemyPawn.id;
        bookmark.selectedCell = hiddenEnemyPawn.position;

        InputHandler input;
        input.reconcileSelection(bookmark, &hiddenEnemyPawn, nullptr, context);

        expect(input.getSelectedPieceId() == -1,
            "Reconciling a hidden enemy piece selection should demote the selection instead of restoring the concealed piece.");
        expect(input.hasSelectedCell() && input.getSelectedCell() == hiddenEnemyPawn.position,
            "When fog hides the enemy piece, InputHandler should preserve the clicked terrain cell as the fallback selection.");
    }

    void testXPSystemUsesDeterministicSerializedSequence() {
        GameConfig config = makeXPTestConfig(
            "      \"sources\": {\n"
            "        \"kill_pawn\": {\n"
            "          \"mean\": 20,\n"
            "          \"sigma_multiplier_times_100\": 50,\n"
            "          \"clamp_sigma_multiplier_times_100\": 175,\n"
            "          \"minimum\": 4\n"
            "        }\n"
            "      }");

        XPSystemState firstState{};
        XPSystemState replayState{};
        const std::uint32_t worldSeed = 424242u;

        const int firstRollA = XPSystem::sampleKillXP(PieceType::Pawn, firstState, worldSeed, config);
        const int secondRollA = XPSystem::sampleKillXP(PieceType::Pawn, firstState, worldSeed, config);
        const int firstRollB = XPSystem::sampleKillXP(PieceType::Pawn, replayState, worldSeed, config);
        const int secondRollB = XPSystem::sampleKillXP(PieceType::Pawn, replayState, worldSeed, config);

        expect(firstRollA == firstRollB && secondRollA == secondRollB,
            "XP sampling should be deterministic for the same world seed and serialized RNG state.");
        expect(firstState.rngCounter == 2 && replayState.rngCounter == 2,
            "XP sampling should advance the serialized RNG counter once per probabilistic reward.");
        expect(firstRollA >= 4 && secondRollA >= 4,
            "XP sampling should respect the configured minimum reward.");

        XPSystemState resumedState = firstState;
        const int resumedRoll = XPSystem::sampleKillXP(PieceType::Pawn, resumedState, worldSeed, config);
        const int continuedRoll = XPSystem::sampleKillXP(PieceType::Pawn, firstState, worldSeed, config);
        expect(resumedRoll == continuedRoll,
            "Restoring the serialized XP RNG state should resume the exact same reward sequence.");
    }

    void testForwardModelCaptureXPMatchesCommittedTurn() {
        GameConfig config = makeXPTestConfig(
            "      \"sources\": {\n"
            "        \"kill_pawn\": {\n"
            "          \"mean\": 20,\n"
            "          \"sigma_multiplier_times_100\": 50,\n"
            "          \"clamp_sigma_multiplier_times_100\": 175,\n"
            "          \"minimum\": 4\n"
            "        }\n"
            "      }");

        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        addPieceToBoard(white, board, 101, PieceType::Rook, KingdomId::White, {4, 4});
        addPieceToBoard(white, board, 102, PieceType::King, KingdomId::White, {1, 1});
        addPieceToBoard(black, board, 201, PieceType::Pawn, KingdomId::Black, {4, 7});
        addPieceToBoard(black, board, 202, PieceType::King, KingdomId::Black, {10, 10});

        const std::uint32_t worldSeed = 919191u;
        GameSnapshot snapshot = ForwardModel::createSnapshot(
            board,
            white,
            black,
            {},
            1,
            worldSeed,
            XPSystemState{});
        expect(ForwardModel::applyMove(snapshot, 101, {4, 7}, KingdomId::White, config),
            "ForwardModel should accept the deterministic XP comparison capture move.");

        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        turnSystem.setTurnNumber(1);
        EventLog eventLog;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;
        ChestSystemState chestSystemState{};
        XPSystemState runtimeXPState{};
        std::vector<MapObject> mapObjects;
        std::vector<AutonomousUnit> autonomousUnits;
        InfernalSystemState infernalSystemState{};
        std::vector<Building> publicBuildings;
        std::vector<GameplayNotification> gameplayNotifications;

        expect(turnSystem.queueCommand(makeMoveCommand(101, {4, 4}, {4, 7}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config),
            "Authoritative runtime should accept the deterministic XP comparison capture move.");

        turnSystem.commitTurn(board,
                              white,
                              black,
                              publicBuildings,
                              mapObjects,
                              chestSystemState,
                              runtimeXPState,
                              autonomousUnits,
                              infernalSystemState,
                              worldSeed,
                              config,
                              eventLog,
                              gameplayNotifications,
                              pieceFactory,
                              buildingFactory);

        const Piece* runtimeRook = white.getPieceById(101);
        const SnapPiece* projectedRook = snapshot.kingdom(KingdomId::White).getPieceById(101);
        expect(runtimeRook != nullptr && projectedRook != nullptr,
            "Both authoritative and projected states should still contain the capturing piece.");
        expect(runtimeRook->xp == projectedRook->xp,
            "ForwardModel capture projection should grant the same deterministic XP as authoritative turn commit.");
        expect(runtimeXPState.rngCounter == snapshot.xpSystemState.rngCounter,
            "ForwardModel capture projection should consume the same XP RNG sequence as authoritative turn commit.");
        expect(black.getPieceById(201) == nullptr,
            "Authoritative turn commit should remove the captured victim after awarding XP.");
    }

    void testInfernalBloodDebtAccumulatesFromCommittedCaptures() {
        GameConfig config = makeInfernalTestConfig("      \"blood_debt_pawn\": 7");
        Board board;
        board.init(12);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        addPieceToBoard(white, board, 821, PieceType::Rook, KingdomId::White, {4, 4});
        addPieceToBoard(white, board, 822, PieceType::King, KingdomId::White, {1, 1});
        addPieceToBoard(black, board, 921, PieceType::Pawn, KingdomId::Black, {4, 7});
        addPieceToBoard(black, board, 922, PieceType::King, KingdomId::Black, {10, 10});

        std::vector<Building> publicBuildings;
        std::vector<MapObject> mapObjects;
        ChestSystemState chestSystemState{};
        std::vector<AutonomousUnit> autonomousUnits;
        InfernalSystemState infernalSystemState{};

        TurnSystem turnSystem;
        turnSystem.setActiveKingdom(KingdomId::White);
        turnSystem.setTurnNumber(1);
        EventLog eventLog;
        std::vector<GameplayNotification> gameplayNotifications;
        PieceFactory pieceFactory;
        BuildingFactory buildingFactory;
        XPSystemState xpSystemState{};
        const std::uint32_t worldSeed = 789123u;

        expect(turnSystem.queueCommand(makeMoveCommand(821, {4, 4}, {4, 7}),
                                       board,
                                       white,
                                       black,
                                       publicBuildings,
                                       config),
            "The debt accumulation test should queue a legal capture move.");

        turnSystem.commitTurn(board,
                              white,
                              black,
                              publicBuildings,
                              mapObjects,
                              chestSystemState,
                              xpSystemState,
                              autonomousUnits,
                              infernalSystemState,
                              worldSeed,
                              config,
                              eventLog,
                              gameplayNotifications,
                              pieceFactory,
                              buildingFactory);

        expect(infernalSystemState.whiteBloodDebt == 7,
            "Capturing an enemy piece during authoritative turn commit should add infernal blood debt to the attacking kingdom.");
    }

    void testInfernalSystemSpawnsOnBorderAndStartsHunt() {
        GameConfig config = makeInfernalTestConfig(
            "      \"min_spawn_turn\": 0,\n"
            "      \"poisson_lambda_base_times_1000\": 100000,\n"
            "      \"poisson_lambda_per_debt_times_1000\": 0,\n"
            "      \"poisson_lambda_cap_times_1000\": 100000");
        Board board;
        board.init(12);

        std::array<Kingdom, kNumKingdoms> kingdoms{Kingdom(KingdomId::White), Kingdom(KingdomId::Black)};
        addPieceToBoard(kingdoms[static_cast<std::size_t>(KingdomId::White)],
                        board,
                        831,
                        PieceType::King,
                        KingdomId::White,
                        {1, 1});
        addPieceToBoard(kingdoms[static_cast<std::size_t>(KingdomId::White)],
                        board,
                        832,
                        PieceType::Rook,
                        KingdomId::White,
                        {5, 5});
        addPieceToBoard(kingdoms[static_cast<std::size_t>(KingdomId::Black)],
                        board,
                        931,
                        PieceType::King,
                        KingdomId::Black,
                        {10, 10});

        InfernalSystemState infernalSystemState{};
        InfernalSystem::initialize(infernalSystemState, 0, config);
        infernalSystemState.nextSpawnTurn = 0;

        std::optional<AutonomousUnit> spawnedInfernal = InfernalSystem::trySpawnInfernal(
            infernalSystemState,
            board,
            kingdoms,
            123456u,
            0,
            1,
            7001,
            config);

        expect(spawnedInfernal.has_value(),
            "An infernal unit should spawn deterministically for the fixed test seed when the Poisson lambda is forced extremely high.");
        expect(spawnedInfernal->infernal.targetKingdom == KingdomId::White,
            "The spawned infernal should target the only kingdom that currently exposes a non-king piece.");
        expect(spawnedInfernal->infernal.targetPieceId == 832,
            "The spawned infernal should bind to the available non-king target piece.");
        expect(isBoardBorderCell(board, spawnedInfernal->position),
            "Infernal spawning should place the unit on a valid border cell.");

        std::vector<Building> publicBuildings;
        std::vector<MapObject> mapObjects;
        std::vector<AutonomousUnit> autonomousUnits{*spawnedInfernal};
        relinkBoardState(board, kingdoms, publicBuildings, mapObjects, autonomousUnits);

        const sf::Vector2i originalPosition = autonomousUnits.front().position;
        expect(InfernalSystem::actAfterCommittedTurn(infernalSystemState,
                                                     board,
                                                     kingdoms,
                                                     autonomousUnits,
                                                     123456u,
                                                     1,
                                                     1,
                                                     KingdomId::Black,
                                                     config),
            "An infernal that targets White should act immediately after Black ends a committed turn.");
        expect(!autonomousUnits.empty(),
            "The infernal should remain alive after starting its hunt.");
        expect(autonomousUnits.front().position != originalPosition,
            "The infernal should advance away from its border spawn when it begins hunting.");
        expect(autonomousUnits.front().infernal.phase == InfernalPhase::Hunting,
            "The infernal should remain in the hunting phase until it reaches or loses its target.");
    }

    void testInfernalSystemCapturesTargetReturnsAndDespawns() {
        GameConfig config = makeInfernalTestConfig(
            "      \"min_spawn_turn\": 0,\n"
            "      \"respawn_cooldown_turns\": 2,\n"
            "      \"spawn_retry_turns\": 1");
        Board board;
        board.init(12);

        std::array<Kingdom, kNumKingdoms> kingdoms{Kingdom(KingdomId::White), Kingdom(KingdomId::Black)};
        addPieceToBoard(kingdoms[static_cast<std::size_t>(KingdomId::White)],
                        board,
                        841,
                        PieceType::Pawn,
                        KingdomId::White,
                        {2, 1});
        addPieceToBoard(kingdoms[static_cast<std::size_t>(KingdomId::White)],
                        board,
                        842,
                        PieceType::King,
                        KingdomId::White,
                        {1, 3});
        addPieceToBoard(kingdoms[static_cast<std::size_t>(KingdomId::Black)],
                        board,
                        941,
                        PieceType::King,
                        KingdomId::Black,
                        {10, 10});

        std::vector<Building> publicBuildings;
        std::vector<MapObject> mapObjects;
        std::vector<AutonomousUnit> autonomousUnits;
        autonomousUnits.push_back(makeTestInfernalUnit(8001, KingdomId::White, {0, 1}, PieceType::Rook));
        autonomousUnits.front().infernal.targetPieceId = 841;
        autonomousUnits.front().infernal.preferredTargetType = PieceType::Pawn;
        autonomousUnits.front().infernal.phase = InfernalPhase::Hunting;

        InfernalSystemState infernalSystemState{};
        infernalSystemState.activeInfernalUnitId = 8001;
        relinkBoardState(board, kingdoms, publicBuildings, mapObjects, autonomousUnits);

        expect(InfernalSystem::actAfterCommittedTurn(infernalSystemState,
                                                     board,
                                                     kingdoms,
                                                     autonomousUnits,
                                                     424242u,
                                                     1,
                                                     1,
                                                     KingdomId::Black,
                                                     config),
            "The infernal should capture its target when a legal hunting path exists.");
        relinkBoardState(board, kingdoms, publicBuildings, mapObjects, autonomousUnits);

        expect(kingdoms[static_cast<std::size_t>(KingdomId::White)].getPieceById(841) == nullptr,
            "A hunted target should be removed from its kingdom once the infernal reaches it.");
        expect(!autonomousUnits.empty() && autonomousUnits.front().infernal.phase == InfernalPhase::Returning,
            "After capturing its target with no replacement available, the infernal should switch to the return phase.");

        int currentTurnStep = 3;
        for (int iteration = 0; iteration < 4 && !autonomousUnits.empty(); ++iteration) {
            expect(InfernalSystem::actAfterCommittedTurn(infernalSystemState,
                                                         board,
                                                         kingdoms,
                                                         autonomousUnits,
                                                         424242u,
                                                         currentTurnStep,
                                                         2 + iteration,
                                                         KingdomId::Black,
                                                         config),
                "A returning infernal should keep progressing toward the border until it disappears.");
            if (!autonomousUnits.empty()) {
                relinkBoardState(board, kingdoms, publicBuildings, mapObjects, autonomousUnits);
            }
            currentTurnStep += 2;
        }

        expect(autonomousUnits.empty(),
            "A returning infernal should disappear after reaching a legal border exit cell.");
        expect(infernalSystemState.activeInfernalUnitId == -1,
            "Despawning the infernal should clear the active infernal unit id.");
        expect(infernalSystemState.nextSpawnTurn >= currentTurnStep - 2 + 4,
            "Despawning the infernal should schedule a later respawn using the configured cooldown.");
    }

    void testTurnDraftCoordinatorSynchronizesAndKeepsProjectedDraftWhileWaiting() {
        GameConfig config;
        GameEngine engine;
        engine.board().init(12);
        engine.kingdom(KingdomId::White) = Kingdom(KingdomId::White);
        engine.kingdom(KingdomId::Black) = Kingdom(KingdomId::Black);
        engine.publicBuildings().clear();
        engine.turnSystem().setActiveKingdom(KingdomId::White);
        engine.turnSystem().setTurnNumber(1);
        engine.kingdom(KingdomId::White).gold = config.getWoodWallCost() + 10;

        addPieceToBoard(engine.kingdom(KingdomId::White),
                        engine.board(),
                        610,
                        PieceType::King,
                        KingdomId::White,
                        {10, 10});
        addPieceToBoard(engine.kingdom(KingdomId::Black),
                        engine.board(),
                        710,
                        PieceType::King,
                        KingdomId::Black,
                        {18, 18});

        std::optional<TurnCommand> queuedBuild;
        const TurnValidationContext turnContext = engine.makeTurnValidationContext(config);
        for (const sf::Vector2i& origin : engine.board().getAllValidCells()) {
            TurnCommand candidate = makeBuildCommand(BuildingType::WoodWall, origin);
            if (PendingTurnProjection::canAppendCommand(turnContext, {}, candidate, nullptr)) {
                queuedBuild = candidate;
                break;
            }
        }

        expect(queuedBuild.has_value(),
            "TurnDraftCoordinator test should find a legal build origin on the projected board.");

        std::string error;
        expect(engine.replacePendingCommands({*queuedBuild}, config, false, &error), error);
        expect(!engine.turnSystem().getPendingCommands().empty(),
            "TurnDraftCoordinator test should leave a concrete pending command queued before draft synchronization.");

        const int queuedBuildId = engine.turnSystem().getPendingCommands().front().buildId;
        TurnDraft draft;
        std::uint64_t lastRevision = 0;
        InputSelectionBookmark bookmark;
        bookmark.buildingId = queuedBuildId;
        int captureCalls = 0;
        int reconcileCalls = 0;

        TurnDraftSynchronizationContext context{
            TurnDraftRuntimeState{GameState::Playing, true, false, true},
            engine,
            draft,
            lastRevision,
            config,
            TurnDraftSynchronizationCallbacks{
                [&bookmark, &captureCalls]() {
                    ++captureCalls;
                    return bookmark;
                },
                [&bookmark, &reconcileCalls](const InputSelectionBookmark& restoredBookmark) {
                    ++reconcileCalls;
                    expect(restoredBookmark.buildingId == bookmark.buildingId,
                        "TurnDraftCoordinator should reconcile the same stable selection bookmark around draft synchronization.");
                }}};

        TurnDraftCoordinator::ensureUpToDate(context);
        expect(draft.isValid(),
            "TurnDraftCoordinator should rebuild the projected draft when a local player has queued commands in an interactive game state.");
        expect(lastRevision == engine.turnSystem().getPendingStateRevision(),
            "TurnDraftCoordinator should remember the pending-state revision after a successful rebuild.");
        expect(captureCalls == 1 && reconcileCalls == 1,
            "TurnDraftCoordinator should capture and reconcile selection once when it refreshes the projected draft.");
        expect(findBuildingById(draft.kingdom(KingdomId::White), queuedBuildId) != nullptr,
            "TurnDraftCoordinator should expose the queued build in the projected kingdom state after rebuilding the draft.");

        TurnDraftCoordinator::ensureUpToDate(context);
        expect(captureCalls == 1 && reconcileCalls == 1,
            "TurnDraftCoordinator should not rebuild or reconcile the draft again while the pending-state revision is unchanged.");

        context.runtimeState.waitingForRemoteTurnResult = true;
        TurnDraftCoordinator::ensureUpToDate(context);
        expect(draft.isValid(),
            "TurnDraftCoordinator should keep the projected draft visible while a LAN client waits for remote turn confirmation.");
        expect(captureCalls == 1 && reconcileCalls == 1,
            "TurnDraftCoordinator should not churn selection reconciliation just because a submitted LAN client is waiting for host confirmation.");
    }

    void testPendingTurnProjectionMaterializesQueuedBuildWithStableId() {
        GameConfig config;
        Board board;
        board.init(8);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getBarracksCost() + 50;

        addPieceToBoard(white, board, 1, PieceType::King, KingdomId::White, {1, 1});
        addPieceToBoard(white, board, 2, PieceType::Pawn, KingdomId::White, {2, 3});
        addPieceToBoard(black, board, 3, PieceType::King, KingdomId::Black, {6, 6});

        TurnCommand buildCommand = makeBuildCommand(BuildingType::Barracks, {3, 3});
        buildCommand.buildId = 141;
        const std::vector<Building> publicBuildings;
        const std::vector<TurnCommand> commands{buildCommand};

        const PendingTurnProjectionResult projection = PendingTurnProjection::project(
            board, white, black, publicBuildings, 1, commands, config);

        expect(projection.valid,
            "A legal queued build should remain valid in the projected turn state.");

        GameSnapshot snapshot = projection.snapshot;
        SnapBuilding* projectedBuilding = snapshot.kingdom(KingdomId::White).getBuildingById(buildCommand.buildId);
        expect(projectedBuilding != nullptr,
            "Projected pending builds should keep their stable build id in the snapshot.");
        expect(projectedBuilding->isUnderConstruction(),
            "Projected pending builds should remain under construction until turn advancement.");
    }

    void testPendingTurnProjectionRejectsProductionFromUnderConstructionBarracks() {
        GameConfig config;
        Board board;
        board.init(8);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getBarracksCost() + config.getRecruitCost(PieceType::Pawn) + 50;

        addPieceToBoard(white, board, 1, PieceType::King, KingdomId::White, {1, 1});
        addPieceToBoard(white, board, 2, PieceType::Pawn, KingdomId::White, {2, 3});
        addPieceToBoard(black, board, 3, PieceType::King, KingdomId::Black, {6, 6});

        TurnCommand buildCommand = makeBuildCommand(BuildingType::Barracks, {3, 3});
        buildCommand.buildId = 142;

        TurnCommand produceCommand;
        produceCommand.type = TurnCommand::Produce;
        produceCommand.barracksId = buildCommand.buildId;
        produceCommand.produceType = PieceType::Pawn;
        const std::vector<Building> publicBuildings;
        const std::vector<TurnCommand> commands{buildCommand, produceCommand};

        const PendingTurnProjectionResult projection = PendingTurnProjection::project(
            board, white, black, publicBuildings, 1, commands, config);

        expect(!projection.valid,
            "A barracks built this turn should not be usable for production before validation.");
        expect(!projection.errorMessage.empty(),
            "Rejected projected production should include an explanatory error message.");
    }

    void testForwardModelCompletesUnderConstructionBarracksOnTurnAdvance() {
        GameConfig config;
        Board board;
        board.init(8);

        Kingdom white(KingdomId::White);
        Kingdom black(KingdomId::Black);
        white.gold = config.getBarracksCost() + config.getRecruitCost(PieceType::Pawn) + 50;

        addPieceToBoard(white, board, 1, PieceType::King, KingdomId::White, {1, 1});
        addPieceToBoard(white, board, 2, PieceType::Pawn, KingdomId::White, {2, 3});
        addPieceToBoard(black, board, 3, PieceType::King, KingdomId::Black, {6, 6});

        GameSnapshot snapshot = ForwardModel::createSnapshot(board, white, black, {}, 1);
        expect(ForwardModel::applyBuild(snapshot, KingdomId::White, BuildingType::Barracks,
                                        {3, 3},
                                        config.getBuildingWidth(BuildingType::Barracks),
                                        config.getBuildingHeight(BuildingType::Barracks),
                                        0,
                                        config.getBarracksCost(),
                                        StructureIntegrityRules::defaultCellHP(BuildingType::Barracks, config),
                                        config,
                                        -43),
            "ForwardModel should accept a legal barracks build for the active kingdom.");
        expect(!ForwardModel::applyProduce(snapshot, -43, PieceType::Pawn,
                                           config.getRecruitCost(PieceType::Pawn),
                                           config.getProductionTurns(PieceType::Pawn),
                                           KingdomId::White),
            "Under-construction barracks should reject production in the same simulated turn.");

        ForwardModel::advanceTurn(snapshot,
                                  KingdomId::White,
                                  config.getMineIncomePerCellPerTurn(),
                                  config.getFarmIncomePerCellPerTurn(),
                                  config);

        SnapBuilding* barracks = snapshot.kingdom(KingdomId::White).getBuildingById(-43);
        expect(barracks != nullptr && !barracks->isUnderConstruction(),
            "Turn advancement should complete newly built barracks in snapshot simulation.");
        expect(ForwardModel::applyProduce(snapshot, -43, PieceType::Pawn,
                                          config.getRecruitCost(PieceType::Pawn),
                                          config.getProductionTurns(PieceType::Pawn),
                                          KingdomId::White),
            "Completed barracks should become usable for production on later simulated turns.");
    }

    void testUnderConstructionResourceBuildingsDoNotGrantIncome() {
        GameConfig config;
        Board board;
        board.init(8);

        Kingdom white(KingdomId::White);
        Building mine;
        mine.id = 88;
        mine.type = BuildingType::Mine;
        mine.owner = KingdomId::White;
        mine.isNeutral = false;
        mine.origin = {2, 2};
        mine.width = config.getBuildingWidth(BuildingType::Mine);
        mine.height = config.getBuildingHeight(BuildingType::Mine);
        mine.cellHP.assign(mine.width * mine.height, 1);
        mine.cellBreachState.assign(mine.width * mine.height, 0);
        mine.setConstructionState(BuildingState::UnderConstruction);
        linkBuildingOnBoard(mine, board);

        addPieceToBoard(white, board, 615, PieceType::Pawn, KingdomId::White, mine.origin);

        const ResourceIncomeBreakdown breakdown = EconomySystem::calculateResourceIncomeBreakdown(
            mine,
            board,
            config);
        expect(!breakdown.isResourceBuilding,
            "Under-construction resource buildings should not expose active income breakdowns.");
        expect(breakdown.whiteIncome == 0 && breakdown.blackIncome == 0,
            "Under-construction resource buildings should not generate passive income before validation.");
    }

        void testBankruptcyValidationRejectsNegativeEndingGold() {
         GameConfig config;
         Board board;
         board.init(10);

         Kingdom white(KingdomId::White);
         Kingdom black(KingdomId::Black);
         addPieceToBoard(white, board, 810, PieceType::King, KingdomId::White, {8, 8});
         addPieceToBoard(white, board, 811, PieceType::Queen, KingdomId::White, {7, 8});
         addPieceToBoard(black, board, 910, PieceType::King, KingdomId::Black, {2, 2});

         const CheckTurnValidation validation = CheckResponseRules::validatePendingTurn(
             white, black, board, {}, 1, {}, config);
         expect(!validation.valid,
             "End-turn validation should reject turns that would finish with negative gold after upkeep.");
         expect(validation.bankrupt,
             "End-turn validation should flag bankruptcy when upkeep would drive gold below zero.");
         expect(validation.projectedEndingGold == -config.getPieceUpkeepCost(PieceType::Queen),
             "Projected ending gold should include upkeep for non-king pieces.");
        }

        void testQueuedDisbandResolvesBankruptcyAndCommitsRemoval() {
         GameConfig config;
         Board board;
         board.init(10);

         Kingdom white(KingdomId::White);
         Kingdom black(KingdomId::Black);
         addPieceToBoard(white, board, 820, PieceType::King, KingdomId::White, {8, 8});
         Piece& rook = addPieceToBoard(white, board, 821, PieceType::Rook, KingdomId::White, {7, 8});
         addPieceToBoard(black, board, 920, PieceType::King, KingdomId::Black, {2, 2});

         TurnSystem turnSystem;
         turnSystem.setActiveKingdom(KingdomId::White);
         expect(turnSystem.queueCommand(makeDisbandCommand(rook.id),
                            board,
                            white,
                            black,
                            {},
                            config),
             "The turn system should accept a queued sacrifice for a non-king piece.");

         const CheckTurnValidation validation = CheckResponseRules::validatePendingTurn(
             white, black, board, {}, 1, turnSystem.getPendingCommands(), config);
         expect(validation.valid,
             "Sacrificing the upkeep-bearing piece should resolve bankruptcy before end-turn validation.");
         expect(!validation.bankrupt,
             "A queued sacrifice that removes upkeep should clear the bankruptcy flag.");

         EventLog eventLog;
         PieceFactory pieceFactory;
         BuildingFactory buildingFactory;
         std::vector<Building> publicBuildings;
         turnSystem.commitTurn(board, white, black, publicBuildings, config, eventLog, pieceFactory, buildingFactory);
         expect(white.getPieceById(rook.id) == nullptr,
             "Committing a queued sacrifice should remove the targeted piece from the kingdom.");
         expect(board.getCell(7, 8).piece == nullptr,
             "Committing a queued sacrifice should clear the piece from the board.");
        }

        void testInGameViewModelIncludesQueuedDisbandAction() {
         GameConfig config;
         GameEngine engine;
         engine.board().init(10);
         engine.kingdom(KingdomId::White) = Kingdom(KingdomId::White);
         engine.kingdom(KingdomId::Black) = Kingdom(KingdomId::Black);
         engine.turnSystem().setActiveKingdom(KingdomId::White);
         engine.turnSystem().setTurnNumber(1);

         addPieceToBoard(engine.kingdom(KingdomId::White), engine.board(), 830, PieceType::King, KingdomId::White, {8, 8});
         Piece& whiteRook = addPieceToBoard(engine.kingdom(KingdomId::White), engine.board(), 831, PieceType::Rook, KingdomId::White, {7, 8});
         addPieceToBoard(engine.kingdom(KingdomId::Black), engine.board(), 930, PieceType::King, KingdomId::Black, {2, 2});

         expect(engine.turnSystem().queueCommand(makeDisbandCommand(whiteRook.id),
                                  engine.board(),
                                  engine.kingdom(KingdomId::White),
                                  engine.kingdom(KingdomId::Black),
                                  engine.publicBuildings(),
                                  config),
             "The test setup should be able to queue a sacrifice before building the dashboard model.");

         const InGameViewModel model = buildInGameViewModel(
             engine,
             config,
             GameState::Playing,
             true,
             makeHudPresentation(KingdomId::White));
         const auto queuedDisband = std::find_if(model.plannedActionRows.begin(),
                                  model.plannedActionRows.end(),
                                  [](const InGamePlannedActionRow& row) {
                                   return row.kindLabel == "Queued"
                                    && row.actionLabel == "Tuer la piece"
                                    && row.detailLabel == "Rook";
                                  });
         expect(queuedDisband != model.plannedActionRows.end(),
             "The dashboard model should list queued sacrifices in the planned actions section.");
        }

    void testInGameViewModelIncludesPlannedActionsAndAutomaticCoronation() {
        GameConfig config;
        GameEngine engine;
        engine.board().init(12);
        engine.kingdom(KingdomId::White) = Kingdom(KingdomId::White);
        engine.kingdom(KingdomId::Black) = Kingdom(KingdomId::Black);
        engine.publicBuildings().clear();
        engine.turnSystem().setActiveKingdom(KingdomId::White);
        engine.turnSystem().setTurnNumber(1);

        engine.publicBuildings().push_back(makeTestPublicBuilding(BuildingType::Church, {10, 10}, 4, 3));
        linkBuildingOnBoard(engine.publicBuildings().back(), engine.board());

        Kingdom& white = engine.kingdom(KingdomId::White);
        Kingdom& black = engine.kingdom(KingdomId::Black);
        addPieceToBoard(white, engine.board(), 600, PieceType::King, KingdomId::White, {10, 10});
        addPieceToBoard(white, engine.board(), 601, PieceType::Bishop, KingdomId::White, {11, 10});
        addPieceToBoard(white, engine.board(), 602, PieceType::Rook, KingdomId::White, {12, 10});
        Piece& pawn = addPieceToBoard(white, engine.board(), 603, PieceType::Pawn, KingdomId::White, {8, 8});
        addPieceToBoard(black, engine.board(), 700, PieceType::King, KingdomId::Black, {4, 4});
        pawn.xp = config.getXPThresholdPawnToKnightOrBishop();
        white.gold = config.getUpgradeCost(PieceType::Pawn, PieceType::Knight) + 10;

        TurnCommand upgradeCommand;
        upgradeCommand.type = TurnCommand::Upgrade;
        upgradeCommand.upgradePieceId = pawn.id;
        upgradeCommand.upgradeTarget = PieceType::Knight;
        expect(engine.turnSystem().queueCommand(upgradeCommand,
                              engine.board(),
                              white,
                              black,
                              engine.publicBuildings(),
                              config),
            "The test setup should be able to queue an upgrade before building the dashboard model.");

        const InGameViewModel model = buildInGameViewModel(
            engine,
            config,
            GameState::Playing,
            true,
            makeHudPresentation(KingdomId::White));
        const auto queuedUpgrade = std::find_if(model.plannedActionRows.begin(),
                              model.plannedActionRows.end(),
                              [](const InGamePlannedActionRow& row) {
                                  return row.kindLabel == "Queued"
                                   && row.actionLabel == "Upgrade Pawn"
                                   && row.detailLabel == "Pawn -> Knight";
                              });
        expect(queuedUpgrade != model.plannedActionRows.end(),
            "The dashboard model should list queued upgrades in the planned actions section.");

        const auto automaticCoronation = std::find_if(model.plannedActionRows.begin(),
                                 model.plannedActionRows.end(),
                                 [](const InGamePlannedActionRow& row) {
                                     return row.kindLabel == "Auto"
                                      && row.actionLabel == "Church coronation";
                                 });
        expect(automaticCoronation != model.plannedActionRows.end(),
            "The dashboard model should expose automatic church coronation when it will trigger at validation.");
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

void testGameStateValidatorRejectsUnderConstructionSaveBuilding() {
    GameConfig config;
    GameEngine engine;
    GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "under_construction_save_test");

    std::string error;
    expect(engine.startNewSession(session, config, &error), error);

    SaveData data = engine.createSaveData();
    Building barracks = makeTestBarracks(77, KingdomId::White, {2, 2}, config);
    barracks.setConstructionState(BuildingState::UnderConstruction);
    data.kingdoms[kingdomIndex(KingdomId::White)].buildings.push_back(barracks);

    expect(!GameStateValidator::validateSaveData(data, &error),
           "Save validation should reject under-construction buildings in authoritative save data.");
    expect(!error.empty(), "Save validation should explain under-construction building failures.");

    GameEngine restored;
    expect(!restored.restoreFromSave(data, config, &error),
           "Restoring a save with under-construction buildings should fail validation.");
}

void testGameStateValidatorRejectsUnderConstructionRuntimeBuilding() {
    GameConfig config;
    GameEngine engine;
    GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "under_construction_runtime_test");

    std::string error;
    expect(engine.startNewSession(session, config, &error), error);

    Building barracks = makeTestBarracks(78, KingdomId::White, {2, 2}, config);
    barracks.setConstructionState(BuildingState::UnderConstruction);
    engine.kingdom(KingdomId::White).addBuilding(barracks);
    relinkBoardState(engine.board(), engine.kingdoms(), engine.publicBuildings());

    expect(!engine.validate(&error),
           "Runtime validation should reject under-construction buildings in authoritative state.");
    expect(!error.empty(), "Runtime validation should explain under-construction building failures.");
}

    void testInfernalAutonomousUnitManifestedTypePersistsAcrossSaveLoad() {
        GameConfig config;
        GameEngine engine;
        GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "infernal_manifested_piece_save_test");

        std::string error;
        expect(engine.startNewSession(session, config, &error), error);

        engine.autonomousUnits().push_back(makeTestInfernalUnit(6601, KingdomId::Black, {3, 5}, PieceType::Bishop));
        relinkBoardState(engine.board(),
                engine.kingdoms(),
                engine.publicBuildings(),
                engine.mapObjects(),
                engine.autonomousUnits());

        SaveManager saveManager;
        SaveData saved = engine.createSaveData();
        saved.infernalSystemState.activeInfernalUnitId = 6601;
        const std::filesystem::path tempPath =
         std::filesystem::temp_directory_path() / "anormalchess_infernal_manifested_piece_test.json";
        expect(saveManager.save(tempPath.string(), saved),
            "SaveManager should serialize infernal autonomous units with their manifested piece type.");

        SaveData loaded;
        expect(saveManager.load(tempPath.string(), loaded),
            "SaveManager should load infernal autonomous units from a serialized save file.");
        std::filesystem::remove(tempPath);

        expect(loaded.autonomousUnits.size() == 1,
            "The serialized save should preserve the infernal autonomous unit entry.");
        expect(loaded.autonomousUnits.front().infernal.manifestedPieceType == PieceType::Bishop,
            "The infernal autonomous unit should preserve its manifested piece archetype across save/load.");

        GameEngine restored;
        expect(restored.restoreFromSave(loaded, config, &error), error);
        expect(restored.autonomousUnits().size() == 1,
            "Restoring a save should rebuild the persisted infernal autonomous unit.");
        expect(restored.autonomousUnits().front().infernal.manifestedPieceType == PieceType::Bishop,
            "Restoring a save should keep the infernal autonomous unit manifested piece type intact.");
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
            << "    \"barracks_repair_cost_per_cell\": -7,\n"
            << "    \"arena_repair_cost_per_cell\": -7,\n"
            << "    \"bridge_repair_cost_per_cell\": -5,\n"
            << "    \"pawn_recruit_cost\": -10,\n"
            << "    \"knight_recruit_cost\": -30,\n"
            << "    \"bishop_recruit_cost\": -30,\n"
            << "    \"rook_recruit_cost\": -60,\n"
            << "    \"pawn_upkeep_cost\": -1,\n"
            << "    \"knight_upkeep_cost\": -2,\n"
            << "    \"bishop_upkeep_cost\": -2,\n"
            << "    \"rook_upkeep_cost\": -4,\n"
            << "    \"queen_upkeep_cost\": -7,\n"
            << "    \"king_upkeep_cost\": -1,\n"
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
        expect(config.getRepairCostPerCell(BuildingType::Barracks) == 0,
            "Negative barracks repair costs should be clamped to zero.");
        expect(config.getRepairCostPerCell(BuildingType::Arena) == 0,
            "Negative arena repair costs should be clamped to zero.");
        expect(config.getRepairCostPerCell(BuildingType::Bridge) == 0,
            "Negative bridge repair costs should be clamped to zero.");
    expect(config.getRecruitCost(PieceType::Pawn) == 0, "Negative recruit costs should be clamped to zero.");
    expect(config.getRecruitCost(PieceType::Knight) == 0, "Negative recruit costs should be clamped to zero.");
    expect(config.getRecruitCost(PieceType::Bishop) == 0, "Negative recruit costs should be clamped to zero.");
    expect(config.getRecruitCost(PieceType::Rook) == 0, "Negative recruit costs should be clamped to zero.");
    expect(config.getPieceUpkeepCost(PieceType::Pawn) == 0, "Negative upkeep costs should be clamped to zero.");
    expect(config.getPieceUpkeepCost(PieceType::Knight) == 0, "Negative upkeep costs should be clamped to zero.");
    expect(config.getPieceUpkeepCost(PieceType::Bishop) == 0, "Negative upkeep costs should be clamped to zero.");
    expect(config.getPieceUpkeepCost(PieceType::Rook) == 0, "Negative upkeep costs should be clamped to zero.");
    expect(config.getPieceUpkeepCost(PieceType::Queen) == 0, "Negative upkeep costs should be clamped to zero.");
    expect(config.getPieceUpkeepCost(PieceType::King) == 0, "Negative upkeep costs should be clamped to zero.");
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
    board.init(4);

    Kingdom white(KingdomId::White);
    Kingdom black(KingdomId::Black);

    Building mine = makeTestPublicBuilding(BuildingType::Mine, {1, 1}, 2, 2);

    std::vector<Building> publicBuildings = {mine};
    addPieceToBoard(white, board, 0, PieceType::Pawn, KingdomId::White, {1, 1});
    addPieceToBoard(white, board, 1, PieceType::Pawn, KingdomId::White, {2, 1});
    addPieceToBoard(white, board, 2, PieceType::Pawn, KingdomId::White, {1, 2});
    addPieceToBoard(black, board, 3, PieceType::Pawn, KingdomId::Black, {2, 2});

    const ResourceIncomeBreakdown dominantIncome = EconomySystem::calculateResourceIncomeBreakdown(
     publicBuildings.front(), board, config);
    expect(dominantIncome.whiteOccupiedCells == 3 && dominantIncome.blackOccupiedCells == 1,
        "Resource breakdown should count occupied cells for both kingdoms on a public resource building.");
    expect(dominantIncome.whiteIncome == 2 * config.getMineIncomePerCellPerTurn(),
        "The leading kingdom should earn income from its net occupation advantage only.");
    expect(dominantIncome.blackIncome == 0,
        "The trailing kingdom should be clamped to zero income on a contested public resource building.");

    const int projectedIncome = EconomySystem::calculateProjectedIncome(white, board, publicBuildings, config);
    expect(projectedIncome == 2 * config.getMineIncomePerCellPerTurn(),
        "Projected income should follow the public resource net occupation rule.");

    const int trailingIncome = EconomySystem::calculateProjectedIncome(black, board, publicBuildings, config);
    expect(trailingIncome == 0,
        "Projected income should clamp to zero for the trailing kingdom on a contested public resource building.");

    for (const sf::Vector2i& pos : publicBuildings.front().getOccupiedCells()) {
     board.getCell(pos.x, pos.y).piece = nullptr;
    }
    white.pieces.clear();
    black.pieces.clear();

    addPieceToBoard(white, board, 4, PieceType::Pawn, KingdomId::White, {1, 1});
    addPieceToBoard(white, board, 5, PieceType::Pawn, KingdomId::White, {2, 1});
    addPieceToBoard(black, board, 6, PieceType::Pawn, KingdomId::Black, {1, 2});
    addPieceToBoard(black, board, 7, PieceType::Pawn, KingdomId::Black, {2, 2});

    const ResourceIncomeBreakdown equalIncome = EconomySystem::calculateResourceIncomeBreakdown(
     publicBuildings.front(), board, config);
    expect(equalIncome.whiteIncome == 0 && equalIncome.blackIncome == 0,
        "Equal occupation on a public resource building should yield zero income for both kingdoms.");
}

void testResourceIncomeHelperSupportsBothResourceTypes() {
    GameConfig config;

    const ResourceIncomeBreakdown farmIncome = EconomySystem::calculateResourceIncomeFromOccupation(
     4, 1, config.getFarmIncomePerCellPerTurn());
    const ResourceIncomeBreakdown mineIncome = EconomySystem::calculateResourceIncomeFromOccupation(
     2, 0, config.getMineIncomePerCellPerTurn());

    expect(farmIncome.whiteIncome == 3 * config.getFarmIncomePerCellPerTurn(),
        "Farm income helper should preserve the configured farm per-cell rate.");
    expect(mineIncome.whiteIncome == 2 * config.getMineIncomePerCellPerTurn(),
        "Mine income helper should preserve the configured mine per-cell rate.");
}

    void testStructureChunkRegistry() {
        const StructureChunkDefinition* church = StructureChunkRegistry::find(BuildingType::Church);
        expect(church != nullptr, "Church should have a chunked structure definition.");
        expect(church->width == 4 && church->height == 3,
            "Church chunk definition should expose the expected 4x3 footprint.");
        const StructureChunkDefinition* barracks = StructureChunkRegistry::find(BuildingType::Barracks);
        expect(barracks != nullptr, "Barracks should now have a chunked structure definition.");
        expect(barracks->width == 4 && barracks->height == 3,
            "Barracks chunk definition should expose the expected 4x3 footprint.");
        const StructureChunkDefinition* arena = StructureChunkRegistry::find(BuildingType::Arena);
        expect(arena != nullptr, "Arena should now have a chunked structure definition.");
        expect(arena->width == 4 && arena->height == 4,
            "Arena chunk definition should expose the expected 4x4 footprint.");
        expect(StructureChunkRegistry::makeChunkTextureRelativePath(BuildingType::Farm, 5, 3)
             == "/textures/cells/structures/farm/farm_6_4.png",
            "Farm chunk path generation should match the runtime asset layout.");
        expect(StructureChunkRegistry::makeChunkTextureKey(BuildingType::Mine, 5, 5)
             == "building_chunk_mine_6_6",
            "Mine chunk keys should be generated from local coordinates.");
        expect(StructureChunkRegistry::makeChunkTextureRelativePath(BuildingType::Barracks, 3, 2)
             == "/textures/cells/structures/barracks/barracks_4_3.png",
            "Barracks chunk paths should match the runtime asset layout.");
        expect(StructureChunkRegistry::makeChunkTextureKey(BuildingType::Arena, 3, 3)
             == "building_chunk_arena_4_4",
            "Arena chunk keys should be generated from local coordinates.");
    }

    void testGameConfigAlignsChunkedStructureDimensions() {
        const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "anormalchess_structure_size_test.json";
        {
         std::ofstream out(tempPath);
         out << "{\n"
             << "  \"buildings\": {\n"
             << "    \"barracks_width\": 1,\n"
             << "    \"barracks_height\": 1,\n"
             << "    \"church_width\": 4,\n"
             << "    \"church_height\": 3,\n"
             << "    \"mine_width\": 6,\n"
             << "    \"mine_height\": 6,\n"
             << "    \"farm_width\": 4,\n"
             << "    \"farm_height\": 3,\n"
             << "    \"arena_width\": 2,\n"
             << "    \"arena_height\": 2\n"
             << "  }\n"
             << "}\n";
        }

        GameConfig config;
        expect(config.loadFromFile(tempPath.string()), "GameConfig should load a partial JSON override file.");
        std::filesystem::remove(tempPath);

        expect(config.getBuildingWidth(BuildingType::Barracks) == 4,
            "Chunked barracks definitions should force the runtime footprint width to 4.");
        expect(config.getBuildingHeight(BuildingType::Barracks) == 3,
            "Chunked barracks definitions should force the runtime footprint height to 3.");
        expect(config.getBuildingWidth(BuildingType::Farm) == 6,
            "Chunked farm definitions should force the runtime footprint width to 6.");
        expect(config.getBuildingHeight(BuildingType::Farm) == 4,
            "Chunked farm definitions should force the runtime footprint height to 4.");
        expect(config.getBuildingWidth(BuildingType::Arena) == 4,
            "Chunked arena definitions should force the runtime footprint width to 4.");
        expect(config.getBuildingHeight(BuildingType::Arena) == 4,
            "Chunked arena definitions should force the runtime footprint height to 4.");
    }

void testInGameViewModelBuilder() {
    GameConfig config;
    GameEngine engine;
    GameSessionConfig session = makeDefaultGameSessionConfig(GameMode::HumanVsHuman, "view_model_test");

    std::string error;
    expect(engine.startNewSession(session, config, &error), error);
    engine.eventLog().log(1, KingdomId::White, "Opened with a pawn move.");

    const InGameViewModel model = buildInGameViewModel(
        engine,
        config,
        GameState::Playing,
        true,
        makeHudPresentation(engine.turnSystem().getActiveKingdom()));
    expect(model.turnNumber == 1, "Dashboard model should expose the active turn number.");
    expect(model.balanceMetrics[0].label == "Gold", "Dashboard model should expose kingdom balance labels.");
        expect(model.eventRows.size() >= 2, "Dashboard model should expose event history rows.");
        expect(model.eventRows.back().actionLabel == "Opened with a pawn move.",
            "Dashboard model should preserve chronological event text.");
        expect(model.eventRows.back().actorLabel.find("Player 1") != std::string::npos,
           "Event rows should use participant names in actor labels.");
    expect(!model.activeTurnLabel.empty(), "Dashboard model should expose the active turn label.");
    expect(model.showTurnPointIndicators,
        "The dashboard model should default to showing turn-point indicators when requested.");
    expect(model.turnIndicatorTone == InGameTurnIndicatorTone::Neutral,
        "The dashboard model should default to a neutral top turn-indicator tone.");
}

}

int main() {
    const std::vector<std::pair<std::string, void(*)()>> tests = {
        {"session defaults", testSessionConfigDefaults},
        {"session validator", testSessionValidatorRejectsInvalidOrdering},
        {"multiplayer validator controllers", testSessionValidatorRejectsInvalidMultiplayerControllers},
        {"multiplayer validator port", testSessionValidatorRejectsInvalidMultiplayerPort},
        {"local player context", testLocalPlayerContextModes},
        {"single local hud modes", testSingleLocalHudModes},
        {"multiplayer runtime reconnect state", testMultiplayerRuntimeReconnectStateLifecycle},
        {"session flow roundtrip", testSessionFlowStartsSavesAndLoadsSession},
        {"session runtime coordinator flow", testSessionRuntimeCoordinatorAppliesSessionEntryAndMainMenuTransitions},
        {"selection query coordinator bookmark fallback", testSelectionQueryCoordinatorResolvesBookmarkFallback},
        {"ui callback coordinator guards", testUICallbackCoordinatorGuardsHudAndToolbarActions},
        {"frontend coordinator hud and waiting lock", testFrontendCoordinatorBuildsHudAndLocksWaitingTurns},
        {"interactive permissions cache reuse", testInteractivePermissionsCacheReusesMatchingRuntimeState},
        {"interactive permissions cache invalidation", testInteractivePermissionsCacheInvalidatesOnStateChangeAndManualReset},
        {"pending turn validation cache reuse", testPendingTurnValidationCacheReusesMatchingTurnState},
        {"pending turn validation cache invalidation", testPendingTurnValidationCacheInvalidatesOnKeyChangesAndManualReset},
        {"frontend coordinator dashboard and panel presentation", testFrontendCoordinatorBuildsProjectedDashboardAndPiecePanel},
        {"multiplayer event coordinator plans", testMultiplayerEventCoordinatorPlansAlertsAndDisconnects},
        {"multiplayer event coordinator restore and cleanup", testMultiplayerEventCoordinatorRestoresSnapshotsAndClientState},
        {"multiplayer runtime coordinator dialog actions", testMultiplayerRuntimeCoordinatorBuildsDialogActions},
        {"input coordinator gameplay shortcuts", testInputCoordinatorPlansGameplayShortcuts},
        {"input coordinator cheatcode shortcuts", testInputCoordinatorPlansCheatcodeShortcuts},
        {"input coordinator world routing", testInputCoordinatorRoutesWorldInputAfterGuiFiltering},
        {"render coordinator move overlay plan", testRenderCoordinatorBuildsSelectionAndMoveOverlayPlan},
        {"weather config structured parameters", testWeatherConfigLoadsStructuredParameters},
        {"cheatcode config boolean and shortcuts", testCheatcodeConfigLoadsBooleanAndShortcuts},
        {"xp config structured profiles", testXPConfigLoadsStructuredProfiles},
        {"weather deterministic serialized sequence", testWeatherSystemUsesDeterministicSerializedSequence},
        {"weather configured speed blocks per 100 turns", testWeatherSystemUsesConfiguredSpeedBlocksPer100Turns},
        {"game engine cheatcode event triggers", testGameEngineCheatcodeTriggersSpawnEvents},
        {"xp deterministic serialized sequence", testXPSystemUsesDeterministicSerializedSequence},
        {"xp forward model matches runtime capture", testForwardModelCaptureXPMatchesCommittedTurn},
        {"infernal blood debt from captures", testInfernalBloodDebtAccumulatesFromCommittedCaptures},
        {"infernal spawn and hunt", testInfernalSystemSpawnsOnBorderAndStartsHunt},
        {"turn draft coordinator sync and keep while waiting", testTurnDraftCoordinatorSynchronizesAndKeepsProjectedDraftWhileWaiting},
        {"infernal capture return despawn", testInfernalSystemCapturesTargetReturnsAndDespawns},
        {"render coordinator anchored piece selection frame", testRenderCoordinatorPrefersAnchoredSelectionCellForPieceFrame},
        {"render coordinator build overlay plan", testRenderCoordinatorBuildsBuildPreviewAndPendingBuildPlan},
        {"update coordinator playing tick", testUpdateCoordinatorPlansPlayingTick},
        {"update coordinator paused and menu tick", testUpdateCoordinatorPlansPausedAndMenuTicks},
        {"ai turn coordinator plans", testAITurnCoordinatorPlansStartAndCompletion},
        {"turn coordinator commit plans", testTurnCoordinatorBuildsCommitPlans},
        {"turn coordinator network flow", testTurnCoordinatorRejectsAndAcceptsNetworkTurnFlow},
        {"turn lifecycle coordinator plans", testTurnLifecycleCoordinatorBuildsDispatchAndResetPlans},
        {"multiplayer join coordinator flow", testMultiplayerJoinCoordinatorPreparesReconnectAndRejectsInvalidJoin},
        {"in-game presentation coordinator menu flow", testInGamePresentationCoordinatorPlansMenuTransitions},
        {"session presentation coordinator flow", testSessionPresentationCoordinatorPlansSessionEntryAndMenuReturn},
        {"panel action coordinator flow", testPanelActionCoordinatorQueuesAndCancelsPieceAndBarracksActions},
        {"engine restore factory sync", testGameEngineRestoresFactoryIds},
        {"engine restore bishop parity", testGameEngineRestoresBishopSpawnMemory},
        {"engine world seed", testGameEngineAssignsWorldSeed},
        {"engine ai staging check fallback", testGameEngineStagesAITurnFallbackMoveWhenInCheck},
        {"engine ai staging bankruptcy recovery", testGameEngineStagesAITurnDisbandsToResolveBankruptcy},
        {"board generator deterministic seed", testBoardGeneratorUsesDeterministicSeed},
        {"board generator terrain balance", testBoardGeneratorProducesGrassDominantTerrain},
        {"board generator grass brightness variation", testBoardGeneratorAppliesDeterministicGrassBrightnessVariation},
        {"board generator centered church", testBoardGeneratorCentersChurchWithoutRotation},
        {"board generator resource dispersion", testBoardGeneratorDispersesPublicResourcesAcrossTypes},
        {"layered selection priority", testLayeredSelectionStackResolvesPriority},
        {"layered selection building cycle", testLayeredSelectionStackSupportsBuildingTerrainCycle},
        {"layered selection preview override", testLayeredSelectionStackSupportsPreviewPieceOverride},
        {"layered selection under construction building", testLayeredSelectionStackTreatsUnderConstructionBuildingAsNormalBuilding},
        {"input bookmarks active piece cell", testInputHandlerBookmarksActivePieceSelectionCell},
        {"input reconcile avoids distant piece jump", testInputHandlerReconcileSelectionDoesNotJumpToMismatchedPieceElsewhere},
        {"input reconcile fogged enemy piece", testInputHandlerReconcileSelectionDemotesFoggedEnemyPieceToTerrain},
        {"input reconcile prefers visible piece over pending override", testInputHandlerReconcileSelectionPrefersVisiblePieceOverPendingMoveOverride},
        {"public building occupation state", testPublicBuildingOccupationStateResolvesAllOutcomes},
        {"cell traversal water blocked", testCellTraversalTreatsWaterAsNotTraversable},
        {"weather visibility allies visible enemies concealed", testWeatherVisibilityKeepsAlliesVisibleAndConcealsEnemies},
        {"private building overlay owner shield", testSelectedStructureOverlayPrivateBuildingsUseOwnerShield},
        {"under construction overlay status row stacks icons", testUnderConstructionStructureOverlayStacksConstructionIconInPrimaryRow},
        {"public building overlay occupation", testSelectedStructureOverlayPublicBuildingsUseOccupationIndicator},
        {"barracks overlay production row", testSelectedStructureOverlayProducingBarracksAddsProgressRow},
        {"check response filters illegal moves", testCheckResponseFiltersMovesThatDoNotResolveCheck},
        {"check response allows blocking move", testCheckResponseAllowsBlockingMove},
        {"check response rejects pass", testCheckResponseRejectsPassWhileInCheck},
        {"check response rejects non-move", testCheckResponseRejectsNonMoveActionsWhileInCheck},
        {"selection move rules unsafe non-king selectable", testSelectionMoveRulesClassifyUnsafeNonKingMovesAsSelectable},
        {"selection move rules unsafe king selectable", testSelectionMoveRulesKeepUnsafeKingSquaresSelectable},
        {"selection move rules ignore queued upgrade live moves", testSelectionMoveRulesIgnoreQueuedUpgradeForLivePieceMoves},
        {"turn system move after queued upgrade", testTurnSystemAllowsMoveAfterQueuedUpgradeBeforeCommit},
        {"hud layout net income wide", testHudLayoutKeepsNetIncomeWide},
        {"toolbar presentation switchers", testToolBarPresentationTracksSwitcherStates},
        {"pending turn unsafe move end-turn rejection", testPendingTurnValidationRejectsUnsafeQueuedMoveOnlyAtEndTurn},
        {"church coronation first rook", testAutomaticChurchCoronationTurnsFirstRookIntoQueen},
        {"church coronation multiple queens", testAutomaticChurchCoronationAllowsMultipleQueens},
        {"church coronation enemy blocked", testAutomaticChurchCoronationBlockedByEnemyPresence},
        {"pawn orthogonal move diagonal capture", testPawnMovesOrthogonallyAndCapturesDiagonally},
        {"pawn diagonal threat map", testPawnThreatSquaresStayDiagonal},
        {"check system pawn diagonal threat", testCheckSystemUsesPawnDiagonalThreats},
        {"pawn diagonal structure capture", testPawnCapturesEnemyStructuresDiagonallyOnly},
        {"forward model pawn semantics", testForwardModelPawnRulesMatchRuntimeSemantics},
        {"check response king sidestep", testCheckResponseAllowsKingSidestepAgainstRookCheck},
        {"check response edge king escape", testCheckResponseAllowsEdgeKingEscapeFromRookCheck},
        {"check response true edge checkmate", testCheckResponseDetectsTrueEdgeCheckmate},
        {"interaction permissions read-only outside turn", testInteractionPermissionsAllowReadOnlyInspectionOutsideTurn},
        {"interaction permissions check build panel", testInteractionPermissionsKeepBuildPanelReadOnlyDuringCheck},
        {"interaction permissions unlock after queued response", testInteractionPermissionsUnlockNonMoveActionsAfterQueuedCheckResponse},
        {"interaction permissions game over navigation", testInteractionPermissionsKeepNavigationAvailableDuringGameOver},
        {"turn system move log piece type", testTurnSystemMoveLogIncludesPieceType},
        {"in-game view model check alert", testInGameViewModelShowsCheckAlertWhenActiveKingIsInCheck},
        {"in-game hud kingdom presentation", testInGameViewModelUsesPresentedHudKingdom},
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
        {"turn system cancel production", testTurnSystemCancelsQueuedProductionPerBarracks},
        {"build system pawn adjacency", testBuildSystemAllowsPawnAdjacency},
        {"turn system repair with pawn occupancy", testTurnSystemRepairsDestroyedOwnedCellWithPawnOccupancy},
        {"turn system repairs before income", testTurnSystemRepairsBeforeIncome},
        {"stone wall destroys after staying breached", testStoneWallDestroysWhenEnemyStaysOnBreachedCellUntilNextCommit},
        {"stone wall breach persists after leaving", testStoneWallBreachPersistsAfterAttackerLeavesAndFinishesOnReturn},
        {"first bishop spawn uses default rule", testFirstBishopSpawnUsesDefaultNearestRule},
        {"bishop spawn alternates across barracks", testBishopSpawnAlternatesAcrossKingdomBarracks},
        {"bishop spawn fallback parity", testBishopSpawnFallsBackWhenPreferredParityUnavailable},
        {"spawn search expands beyond radius two", testSpawnSearchExpandsBeyondInitialRadius},
        {"blocked bishop spawn keeps memory", testBlockedBishopSpawnKeepsKingdomMemoryUnchanged},
        {"simultaneous bishop spawns alternate", testSimultaneousBishopSpawnsAlternateWithinSameTurn},
        {"forward model bishop spawn consistency", testForwardModelMatchesRuntimeBishopSpawnRule},
        {"turn system unaffordable upgrade", testTurnSystemSkipsUnaffordableUpgrade},
        {"turn system cancel upgrade", testTurnSystemCancelsQueuedUpgradePerPiece},
        {"selection move rules pending build reselection", testSelectionMoveRulesKeepMovesWhenPendingBuildDependsOnCurrentMove},
        {"turn system replace move drops orphan build", testTurnSystemReplaceMoveDropsOrphanedPendingBuilds},
        {"turn system queue move drops final unsupported build", testTurnSystemQueueMoveDropsPendingBuildUnsupportedInFinalState},
        {"turn system queue move keeps final supported build", testTurnSystemQueueMoveKeepsPendingBuildWhenAnotherFinalBuilderSupportsIt},
        {"build overlay exposes occupied structure cells", testBuildOverlayRulesExposeOccupiedCellsForMultiCellStructure},
        {"build overlay coverage follows queued move state", testBuildOverlayRulesCoverageFollowsQueuedMoveProjectedState},
        {"structure placement uses configured anchors", testStructurePlacementProfilesUseConfiguredAnchorSourceLocals},
        {"structure placement round trips anchors", testStructurePlacementProfilesRoundTripAnchorConversionAcrossRotations},
        {"build overlay origins match brute force", testBuildOverlayRulesOriginsMatchBruteForceProjection},
        {"build overlay coordinator cache lifecycle", testBuildOverlayCoordinatorRefreshesClearsAndInvalidatesCache},
        {"turn system keep build with other builder", testTurnSystemCancelMoveKeepsBuildWhenAnotherBuilderStillSupportsIt},
        {"turn system stable pending build ids", testTurnSystemAssignsStablePendingBuildIds},
        {"turn system commit preserves reserved build id", testTurnSystemCommitPreservesReservedBuildId},
        {"turn draft materializes under construction build", testTurnDraftMaterializesQueuedBuildingAsUnderConstruction},
        {"turn draft captures autonomous unit", testTurnDraftCapturesAutonomousUnitOnQueuedMove},
        {"turn system captures autonomous unit", testTurnSystemCapturesAutonomousUnitAndClearsInfernalState},
        {"pending turn projection stable build id", testPendingTurnProjectionMaterializesQueuedBuildWithStableId},
        {"pending turn projection rejects under construction produce", testPendingTurnProjectionRejectsProductionFromUnderConstructionBarracks},
        {"forward model completes under construction barracks", testForwardModelCompletesUnderConstructionBarracksOnTurnAdvance},
        {"under construction resources grant no income", testUnderConstructionResourceBuildingsDoNotGrantIncome},
        {"bankruptcy validation rejects negative ending gold", testBankruptcyValidationRejectsNegativeEndingGold},
        {"queued disband resolves bankruptcy", testQueuedDisbandResolvesBankruptcyAndCommitsRemoval},
        {"in-game planned disband action", testInGameViewModelIncludesQueuedDisbandAction},
        {"ai special planning preserves gold", testAIStrategySpecialDoesNotMutateRuntimeGold},
        {"save validator negative gold", testGameStateValidatorRejectsNegativeSaveGold},
        {"runtime validator negative gold", testGameStateValidatorRejectsNegativeRuntimeGold},
        {"save validator under construction building", testGameStateValidatorRejectsUnderConstructionSaveBuilding},
        {"runtime validator under construction building", testGameStateValidatorRejectsUnderConstructionRuntimeBuilding},
        {"infernal save roundtrip manifested piece type", testInfernalAutonomousUnitManifestedTypePersistsAcrossSaveLoad},
        {"game config clamps negative economy", testGameConfigClampsNegativeEconomyValues},
        {"projected income helper", testProjectedIncomeHelper},
        {"resource income helper resource types", testResourceIncomeHelperSupportsBothResourceTypes},
        {"structure chunk registry", testStructureChunkRegistry},
        {"chunked structure dimensions", testGameConfigAlignsChunkedStructureDimensions},
        {"in-game view model builder", testInGameViewModelBuilder},
        {"in-game planned actions", testInGameViewModelIncludesPlannedActionsAndAutomaticCoronation},
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