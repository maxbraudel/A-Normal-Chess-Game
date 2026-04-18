#pragma once

#include <array>
#include <string>
#include <vector>

#include "Autonomous/AutonomousUnit.hpp"
#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingFactory.hpp"
#include "Core/GameplayNotification.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Objects/MapObject.hpp"
#include "Save/SaveData.hpp"
#include "Systems/ChestSystem.hpp"
#include "Systems/CheckResponseRules.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/InfernalSystem.hpp"
#include "Systems/TurnSystem.hpp"
#include "Systems/TurnValidationContext.hpp"
#include "Systems/WeatherTypes.hpp"
#include "Systems/XPTypes.hpp"
#include "Units/PieceFactory.hpp"

class GameConfig;

void relinkBoardState(Board& board,
                      std::array<Kingdom, kNumKingdoms>& kingdoms,
                      std::vector<Building>& publicBuildings,
                      std::vector<MapObject>& mapObjects,
                      std::vector<AutonomousUnit>& autonomousUnits);
void relinkBoardState(Board& board,
                      std::array<Kingdom, kNumKingdoms>& kingdoms,
                      std::vector<Building>& publicBuildings,
                      std::vector<MapObject>& mapObjects);
void relinkBoardState(Board& board,
                      std::array<Kingdom, kNumKingdoms>& kingdoms,
                      std::vector<Building>& publicBuildings);

struct PendingTurnCommitResult {
    bool committed = false;
    bool gameOver = false;
    KingdomId winner = KingdomId::White;
    CheckTurnValidation activeValidation;
    CheckTurnValidation nextTurnValidation;
    std::vector<GameplayNotification> notifications;
};

class GameEngine {
public:
    GameEngine();

    bool startNewSession(const GameSessionConfig& session,
                         const GameConfig& config,
                         std::string* errorMessage = nullptr);
    bool restoreFromSave(const SaveData& data,
                         const GameConfig& config,
                         std::string* errorMessage = nullptr);
    SaveData createSaveData() const;
    bool validate(std::string* errorMessage = nullptr) const;
    void resetPendingTurn();
    bool replacePendingCommands(const std::vector<TurnCommand>& commands,
                               const GameConfig& config,
                               bool assignAuthoritativeBuildIds = false,
                               std::string* errorMessage = nullptr);
    TurnValidationContext makeTurnValidationContext(const GameConfig& config) const;
    CheckTurnValidation validatePendingTurn(const GameConfig& config) const;
    PendingTurnCommitResult commitPendingTurn(const GameConfig& config);
    bool triggerCheatcodeWeatherFront(const GameConfig& config);
    bool triggerCheatcodeChestSpawn(const GameConfig& config);
    bool triggerCheatcodeInfernalSpawn(const GameConfig& config);

    Board& board() { return m_board; }
    const Board& board() const { return m_board; }

    std::array<Kingdom, kNumKingdoms>& kingdoms() { return m_kingdoms; }
    const std::array<Kingdom, kNumKingdoms>& kingdoms() const { return m_kingdoms; }

    std::vector<Building>& publicBuildings() { return m_publicBuildings; }
    const std::vector<Building>& publicBuildings() const { return m_publicBuildings; }

    std::vector<MapObject>& mapObjects() { return m_mapObjects; }
    const std::vector<MapObject>& mapObjects() const { return m_mapObjects; }

    std::vector<AutonomousUnit>& autonomousUnits() { return m_autonomousUnits; }
    const std::vector<AutonomousUnit>& autonomousUnits() const { return m_autonomousUnits; }

    const ChestSystemState& chestSystemState() const { return m_chestSystemState; }
    const WeatherSystemState& weatherSystemState() const { return m_weatherSystemState; }
    WeatherMaskCache& weatherMaskCache() { return m_weatherMaskCache; }
    const WeatherMaskCache& weatherMaskCache() const { return m_weatherMaskCache; }
    const XPSystemState& xpSystemState() const { return m_xpSystemState; }
    const InfernalSystemState& infernalSystemState() const { return m_infernalSystemState; }

    void ensureWeatherMaskUpToDate(const GameConfig& config);
    void clearWeatherMaskCache();

    TurnSystem& turnSystem() { return m_turnSystem; }
    const TurnSystem& turnSystem() const { return m_turnSystem; }

    EventLog& eventLog() { return m_eventLog; }
    const EventLog& eventLog() const { return m_eventLog; }

    PieceFactory& pieceFactory() { return m_pieceFactory; }
    BuildingFactory& buildingFactory() { return m_buildingFactory; }

    const GameSessionConfig& sessionConfig() const { return m_sessionConfig; }
    GameMode gameMode() const { return gameModeFromSession(m_sessionConfig); }
    std::string gameName() const { return m_sessionConfig.saveName; }

    Kingdom& kingdom(KingdomId id) { return m_kingdoms[kingdomIndex(id)]; }
    const Kingdom& kingdom(KingdomId id) const { return m_kingdoms[kingdomIndex(id)]; }

    Kingdom& activeKingdom() { return kingdom(m_turnSystem.getActiveKingdom()); }
    const Kingdom& activeKingdom() const { return kingdom(m_turnSystem.getActiveKingdom()); }

    Kingdom& enemyKingdom() { return kingdom(opponent(m_turnSystem.getActiveKingdom())); }
    const Kingdom& enemyKingdom() const { return kingdom(opponent(m_turnSystem.getActiveKingdom())); }

    ControllerType controller(KingdomId id) const;
    bool isHumanControlled(KingdomId id) const;
    bool isActiveHuman() const;
    std::array<ControllerType, kNumKingdoms> controllers() const;
    std::array<std::string, kNumKingdoms> participantNames() const;
    std::string participantName(KingdomId id) const;
    std::string activeTurnLabel() const;

private:
    void syncFactoryIds();
    bool spawnChestIfDue(const GameConfig& config);
    bool spawnInfernalIfDue(const GameConfig& config,
                           const WeatherMaskCache& weatherMaskCache,
                           int currentTurnStep);

    GameSessionConfig m_sessionConfig = makeDefaultGameSessionConfig(GameMode::HumanVsHuman);
    Board m_board;
    std::array<Kingdom, kNumKingdoms> m_kingdoms;
    std::vector<Building> m_publicBuildings;
    std::vector<MapObject> m_mapObjects;
    std::vector<AutonomousUnit> m_autonomousUnits;
    ChestSystemState m_chestSystemState{};
    WeatherSystemState m_weatherSystemState{};
    WeatherMaskCache m_weatherMaskCache{};
    XPSystemState m_xpSystemState{};
    InfernalSystemState m_infernalSystemState{};
    TurnSystem m_turnSystem;
    EventLog m_eventLog;
    PieceFactory m_pieceFactory;
    BuildingFactory m_buildingFactory;
    int m_nextMapObjectId = 1;
    int m_nextAutonomousUnitId = 1;
};