#pragma once
#include <array>
#include <string>
#include <vector>

#include "Autonomous/AutonomousUnit.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Board/Cell.hpp"
#include "Objects/MapObject.hpp"
#include "Systems/ChestSystem.hpp"
#include "Systems/InfernalSystem.hpp"
#include "Systems/WeatherTypes.hpp"
#include "Systems/XPTypes.hpp"
#include "Units/Piece.hpp"
#include "Buildings/Building.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/TurnCommand.hpp"

struct SaveData {
    std::string gameName;
    int turnNumber = 1;
    KingdomId activeKingdom = KingdomId::White;
    int mapRadius = 50;
    std::uint32_t worldSeed = 0;
    std::array<KingdomParticipantConfig, kNumKingdoms> sessionKingdoms =
        defaultKingdomParticipants(GameMode::HumanVsHuman);
    MultiplayerConfig multiplayer{};
    bool tacticalGridEnabled = false;
    bool sharedTurnPreviewEnabled = false;

    // Grid state
    struct CellData {
        CellType type = CellType::Grass;
        bool isInCircle = false;
        int terrainFlipMask = -1;
        std::uint8_t terrainBrightness = 255;
    };
    std::vector<std::vector<CellData>> grid;

    // Kingdom data (indexed by KingdomId: 0=White, 1=Black)
    struct KingdomData {
        KingdomId id = KingdomId::White;
        int gold = 0;
        int movementPointsMaxBonus = 0;
        int buildPointsMaxBonus = 0;
        bool hasSpawnedBishop = false;
        int lastBishopSpawnParity = 0;
        std::vector<Piece> pieces;
        std::vector<Building> buildings;
    };
    std::array<KingdomData, kNumKingdoms> kingdoms{
        KingdomData{KingdomId::White, 0, 0, 0, false, 0, {}, {}},
        KingdomData{KingdomId::Black, 0, 0, 0, false, 0, {}, {}}
    };

    // Public buildings
    std::vector<Building> publicBuildings;

    // Map objects
    std::vector<MapObject> mapObjects;
    ChestSystemState chestSystemState{};
    WeatherSystemState weatherSystemState{};
    WeatherMaskCache weatherMaskCache{};
    XPSystemState xpSystemState{};

    // Autonomous hostile units
    std::vector<AutonomousUnit> autonomousUnits;
    InfernalSystemState infernalSystemState{};

    // Events
    std::vector<EventLog::Event> events;

    // Command history (for future replay)
    std::vector<TurnCommand> commandHistory;
};
