#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Board/Board.hpp"
#include "Board/CellType.hpp"
#include "Buildings/BuildingFactory.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Core/GameStateValidator.hpp"
#include "Save/SaveManager.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/TurnCommand.hpp"
#include "Systems/TurnSystem.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceFactory.hpp"

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
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
    data.sessionKingdoms = defaultKingdomParticipants(GameMode::AIvsAI);
    data.sessionKingdoms[0].participantName = "AI \"Alpha\"";
    data.sessionKingdoms[1].participantName = "AI Beta";
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
    expect(loaded.controllers[0] == ControllerType::AI && loaded.controllers[1] == ControllerType::AI,
           "Legacy controller metadata should stay aligned with session metadata.");
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

}

int main() {
    const std::vector<std::pair<std::string, void(*)()>> tests = {
        {"session defaults", testSessionConfigDefaults},
        {"session validator", testSessionValidatorRejectsInvalidOrdering},
        {"engine restore factory sync", testGameEngineRestoresFactoryIds},
        {"save manager roundtrip", testSaveManagerRoundTrip},
        {"turn system affordability", testTurnSystemSkipsUnaffordableBuild},
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