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
#include "AI/ForwardModel.hpp"
#include "AI/AIStrategySpecial.hpp"
#include "Board/Board.hpp"
#include "Board/BoardGenerator.hpp"
#include "Board/CellTraversal.hpp"
#include "Board/CellType.hpp"
#include "Buildings/BuildingFactory.hpp"
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
#include "Input/LayeredSelection.hpp"
#include "Render/StructureOverlay.hpp"
#include "Multiplayer/MultiplayerClient.hpp"
#include "Multiplayer/PasswordUtils.hpp"
#include "Multiplayer/Protocol.hpp"
#include "Multiplayer/MultiplayerServer.hpp"
#include "Save/SaveManager.hpp"
#include "Systems/BuildSystem.hpp"
#include "Systems/CheckResponseRules.hpp"
#include "Systems/CheckSystem.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/PendingTurnProjection.hpp"
#include "Systems/ProductionSpawnRules.hpp"
#include "Systems/ProductionSystem.hpp"
#include "Systems/PublicBuildingOccupation.hpp"
#include "Systems/SelectionMoveRules.hpp"
#include "Systems/StructureIntegrityRules.hpp"
#include "Systems/TurnCommand.hpp"
#include "Systems/TurnSystem.hpp"
#include "UI/HUDLayout.hpp"
#include "UI/InGameViewModelBuilder.hpp"
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
        white.gold = config.getUpgradeCost(PieceType::Pawn, PieceType::Bishop) + 5;

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
        expect(white.gold == 5,
            "Deferred upgrades should still reserve and spend their gold by the end of commit.");
    }

    void testHudLayoutKeepsNetIncomeWide() {
        expect(HUDLayout::metricWidths()[3] == HUDLayout::kWideMetricWidth,
            "Net Income should use the wide metric slot so its label fits without overflow.");
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
                              config.getArenaXPPerTurn(),
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
                                  config.getArenaXPPerTurn(),
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
        {"engine restore factory sync", testGameEngineRestoresFactoryIds},
        {"engine restore bishop parity", testGameEngineRestoresBishopSpawnMemory},
        {"engine world seed", testGameEngineAssignsWorldSeed},
        {"board generator deterministic seed", testBoardGeneratorUsesDeterministicSeed},
        {"board generator terrain balance", testBoardGeneratorProducesGrassDominantTerrain},
        {"board generator centered church", testBoardGeneratorCentersChurchWithoutRotation},
        {"board generator resource dispersion", testBoardGeneratorDispersesPublicResourcesAcrossTypes},
        {"layered selection priority", testLayeredSelectionStackResolvesPriority},
        {"layered selection building cycle", testLayeredSelectionStackSupportsBuildingTerrainCycle},
        {"layered selection preview override", testLayeredSelectionStackSupportsPreviewPieceOverride},
        {"layered selection under construction building", testLayeredSelectionStackTreatsUnderConstructionBuildingAsNormalBuilding},
        {"public building occupation state", testPublicBuildingOccupationStateResolvesAllOutcomes},
        {"cell traversal water blocked", testCellTraversalTreatsWaterAsNotTraversable},
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
        {"turn system keep build with other builder", testTurnSystemCancelMoveKeepsBuildWhenAnotherBuilderStillSupportsIt},
        {"turn system stable pending build ids", testTurnSystemAssignsStablePendingBuildIds},
        {"turn system commit preserves reserved build id", testTurnSystemCommitPreservesReservedBuildId},
        {"turn draft materializes under construction build", testTurnDraftMaterializesQueuedBuildingAsUnderConstruction},
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