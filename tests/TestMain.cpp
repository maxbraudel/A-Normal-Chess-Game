#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <SFML/System/Time.hpp>

#include "Board/Board.hpp"
#include "Board/CellType.hpp"
#include "Buildings/BuildingFactory.hpp"
#include "Buildings/StructureChunkRegistry.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Core/LocalPlayerContext.hpp"
#include "Core/GameState.hpp"
#include "Core/GameStateValidator.hpp"
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

void testSaveManagerRoundTrip() {
    SaveData data;
    data.gameName = "save_roundtrip";
    data.turnNumber = 7;
    data.activeKingdom = KingdomId::Black;
    data.mapRadius = 5;
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
            {CellType::Grass, true},
            {CellType::Water, true}
        },
        {
            {CellType::Void, false},
            {CellType::Dirt, true}
        }
    };

    data.kingdoms[0].id = KingdomId::White;
    data.kingdoms[0].gold = 120;
    data.kingdoms[0].pieces.push_back(Piece(0, PieceType::King, KingdomId::White, {0, 0}));
    data.kingdoms[1].id = KingdomId::Black;
    data.kingdoms[1].gold = 95;
    data.kingdoms[1].pieces.push_back(Piece(1, PieceType::King, KingdomId::Black, {1, 0}));
    data.events.push_back({7, KingdomId::Black, "AI said \"check\" and held {center}."});

    SaveManager manager;
    const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "anormalchess_save_test.json";
    expect(manager.save(tempPath.string(), data), "SaveManager should write a save file.");

    SaveData loaded;
    expect(manager.load(tempPath.string(), loaded), "SaveManager should reload the written save.");
    std::filesystem::remove(tempPath);

    expect(loaded.grid.size() == 2 && loaded.grid[0].size() == 2,
           "Grid data should round-trip through SaveManager.");
    expect(loaded.events.size() == 1 && loaded.events[0].message == data.events[0].message,
           "Event messages should preserve escaped characters.");
    expect(loaded.sessionKingdoms[0].participantName == data.sessionKingdoms[0].participantName,
           "Session participant names should round-trip through SaveManager.");
        expect(loaded.controllers[0] == ControllerType::Human && loaded.controllers[1] == ControllerType::Human,
           "Legacy controller metadata should stay aligned with session metadata.");
        expect(loaded.multiplayer.enabled && loaded.multiplayer.port == data.multiplayer.port,
            "Multiplayer metadata should round-trip through SaveManager.");
        expect(loaded.multiplayer.passwordHash == data.multiplayer.passwordHash,
            "Multiplayer password hash should round-trip through SaveManager.");
}

        void testSaveManagerStringRoundTrip() {
            SaveData data;
            data.gameName = "save_string_roundtrip";
            data.turnNumber = 3;
            data.activeKingdom = KingdomId::White;
            data.mapRadius = 4;
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
        {"save manager roundtrip", testSaveManagerRoundTrip},
        {"save manager string roundtrip", testSaveManagerStringRoundTrip},
        {"multiplayer password digest", testMultiplayerPasswordDigest},
        {"multiplayer turn packet roundtrip", testMultiplayerTurnSubmissionPacketRoundTrip},
        {"multiplayer turn rejection packet roundtrip", testMultiplayerTurnRejectedPacketRoundTrip},
        {"multiplayer loopback smoke", testMultiplayerLoopbackSmoke},
        {"turn system affordability", testTurnSystemSkipsUnaffordableBuild},
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