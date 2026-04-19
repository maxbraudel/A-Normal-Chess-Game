#include "Core/GameEngine.hpp"

#include <algorithm>
#include <random>

#include "Autonomous/AutonomousUnit.hpp"
#include "Board/BoardGenerator.hpp"
#include "Config/GameConfig.hpp"
#include "Core/GameStateValidator.hpp"
#include "Systems/StructureIntegrityRules.hpp"
#include "Systems/ChestSystem.hpp"
#include "Systems/InfernalSystem.hpp"
#include "Systems/WeatherSystem.hpp"

namespace {

constexpr int kFlipHorizontalMask = 1;
constexpr int kFlipVerticalMask = 2;

std::uint32_t makeRandomWorldSeed() {
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<std::uint32_t> dist(1u, 0x7fffffffu);
    return dist(generator);
}

std::uint32_t deriveLegacyWorldSeed(const SaveData& data) {
    std::uint32_t hash = 2166136261u;
    auto mix = [&](std::uint32_t value) {
        hash ^= value;
        hash *= 16777619u;
    };

    for (const char current : data.gameName) {
        mix(static_cast<std::uint32_t>(static_cast<unsigned char>(current)));
    }

    mix(static_cast<std::uint32_t>(data.turnNumber));
    mix(static_cast<std::uint32_t>(data.mapRadius));
    mix(static_cast<std::uint32_t>(data.activeKingdom));
    return (hash == 0) ? 1u : (hash & 0x7fffffffu);
}

std::uint32_t mixVisualSeed(std::uint32_t seed, std::uint32_t value) {
    std::uint32_t hash = seed ^ (value + 0x9e3779b9u + (seed << 6) + (seed >> 2));
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16;
    return hash;
}

int deriveLegacyPublicBuildingRotation(std::uint32_t worldSeed, const Building& building) {
    std::uint32_t seed = (worldSeed == 0) ? 1u : worldSeed;
    seed = mixVisualSeed(seed, static_cast<std::uint32_t>(building.type) + 1u);
    seed = mixVisualSeed(seed, static_cast<std::uint32_t>(building.origin.x + 1) * 2654435761u);
    seed = mixVisualSeed(seed, static_cast<std::uint32_t>(building.origin.y + 1) * 2246822519u);
    return static_cast<int>(seed & 0x3u);
}

int deriveLegacyPublicBuildingFlipMask(std::uint32_t worldSeed, const Building& building) {
    std::uint32_t seed = (worldSeed == 0) ? 1u : worldSeed;
    seed = mixVisualSeed(seed, static_cast<std::uint32_t>(building.type) + 17u);
    seed = mixVisualSeed(seed, static_cast<std::uint32_t>(building.origin.x + 1) * 3266489917u);
    seed = mixVisualSeed(seed, static_cast<std::uint32_t>(building.origin.y + 1) * 668265263u);
    return static_cast<int>(seed & (kFlipHorizontalMask | kFlipVerticalMask));
}

int halfTurnStep(const TurnSystem& turnSystem) {
    return ((turnSystem.getTurnNumber() - 1) * 2)
        + kingdomIndex(turnSystem.getActiveKingdom());
}

void normalizeLoadedBuildings(std::vector<Building>& buildings,
                              std::uint32_t worldSeed,
                              const GameConfig& config) {
    for (auto& building : buildings) {
        if (building.rotationQuarterTurns < 0) {
            building.rotationQuarterTurns = building.isPublic()
                ? deriveLegacyPublicBuildingRotation(worldSeed, building)
                : 0;
        }

        if (building.flipMask < 0) {
            building.flipMask = building.isPublic()
                ? deriveLegacyPublicBuildingFlipMask(worldSeed, building)
                : 0;
        }

        if (!building.isPublic()) {
            building.flipMask = 0;
        } else {
            building.flipMask &= (kFlipHorizontalMask | kFlipVerticalMask);
        }

        if (building.destroyedCellsRequired <= 0) {
            building.setDestroyedCellsRequired(
                StructureIntegrityRules::destroyedCellsRequired(building.type, config));
        } else {
            building.setDestroyedCellsRequired(building.destroyedCellsRequired);
        }
    }
}

}

void relinkBoardState(Board& board,
                      std::array<Kingdom, kNumKingdoms>& kingdoms,
                      std::vector<Building>& publicBuildings,
                      std::vector<MapObject>& mapObjects,
                      std::vector<AutonomousUnit>& autonomousUnits) {
    const int diameter = board.getDiameter();
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            Cell& cell = board.getCell(x, y);
            cell.piece = nullptr;
            cell.building = nullptr;
            cell.mapObject = nullptr;
            cell.autonomousUnit = nullptr;
        }
    }

    for (auto& building : publicBuildings) {
        for (const auto& pos : building.getOccupiedCells()) {
            if (board.isInBounds(pos.x, pos.y)) {
                board.getCell(pos.x, pos.y).building = &building;
            }
        }
    }

    for (auto& kingdom : kingdoms) {
        for (auto& building : kingdom.buildings) {
            for (const auto& pos : building.getOccupiedCells()) {
                if (board.isInBounds(pos.x, pos.y)) {
                    board.getCell(pos.x, pos.y).building = &building;
                }
            }
        }

        for (auto& piece : kingdom.pieces) {
            if (board.isInBounds(piece.position.x, piece.position.y)) {
                board.getCell(piece.position.x, piece.position.y).piece = &piece;
            }
        }
    }

    for (auto& mapObject : mapObjects) {
        if (board.isInBounds(mapObject.position.x, mapObject.position.y)) {
            board.getCell(mapObject.position.x, mapObject.position.y).mapObject = &mapObject;
        }
    }

    for (auto& autonomousUnit : autonomousUnits) {
        if (board.isInBounds(autonomousUnit.position.x, autonomousUnit.position.y)) {
            board.getCell(autonomousUnit.position.x, autonomousUnit.position.y).autonomousUnit = &autonomousUnit;
        }
    }
}

void relinkBoardState(Board& board,
                      std::array<Kingdom, kNumKingdoms>& kingdoms,
                      std::vector<Building>& publicBuildings,
                      std::vector<MapObject>& mapObjects) {
    static std::vector<AutonomousUnit> emptyAutonomousUnits;
    emptyAutonomousUnits.clear();
    relinkBoardState(board, kingdoms, publicBuildings, mapObjects, emptyAutonomousUnits);
}

void relinkBoardState(Board& board,
                      std::array<Kingdom, kNumKingdoms>& kingdoms,
                      std::vector<Building>& publicBuildings) {
    static std::vector<MapObject> emptyMapObjects;
    emptyMapObjects.clear();
    relinkBoardState(board, kingdoms, publicBuildings, emptyMapObjects);
}

GameEngine::GameEngine()
    : m_kingdoms{Kingdom(KingdomId::White), Kingdom(KingdomId::Black)} {
}

void GameEngine::relinkRuntimeState() {
    relinkBoardState(m_board, m_kingdoms, m_publicBuildings, m_mapObjects, m_autonomousUnits);
}

bool GameEngine::relinkAndValidateRuntimeState(std::string* errorMessage) {
    relinkRuntimeState();
    return validate(errorMessage);
}

bool GameEngine::startNewSession(const GameSessionConfig& session,
                                 const GameConfig& config,
                                 std::string* errorMessage) {
    if (!GameStateValidator::validateSessionConfig(session, errorMessage)) {
        return false;
    }

    m_sessionConfig = session;
    if (m_sessionConfig.worldSeed == 0) {
        m_sessionConfig.worldSeed = makeRandomWorldSeed();
    }
    m_pieceFactory.reset();
    m_buildingFactory.reset();

    m_board.init(config.getMapRadius());
    m_publicBuildings.clear();
    m_mapObjects.clear();
    m_autonomousUnits.clear();
    const auto generation = BoardGenerator::generate(m_board, config, m_publicBuildings, m_sessionConfig.worldSeed);

    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        const auto kingdomId = static_cast<KingdomId>(kingdomSlot);
        m_kingdoms[kingdomSlot] = Kingdom(kingdomId);
        m_kingdoms[kingdomSlot].gold = config.getStartingGold();
    }

    const Piece whiteKing = m_pieceFactory.createPiece(PieceType::King, KingdomId::White, generation.playerSpawn);
    const Piece blackKing = m_pieceFactory.createPiece(PieceType::King, KingdomId::Black, generation.aiSpawn);
    kingdom(KingdomId::White).addPiece(whiteKing);
    kingdom(KingdomId::Black).addPiece(blackKing);

    ChestSystem::initialize(m_chestSystemState, m_sessionConfig.worldSeed, 1, config);
    m_xpSystemState = XPSystemState{};
    InfernalSystem::initialize(m_infernalSystemState, 0, config);
    relinkRuntimeState();

    m_turnSystem = TurnSystem();
    m_turnSystem.setActiveKingdom(KingdomId::White);
    m_turnSystem.setTurnNumber(1);
    WeatherSystem::initialize(m_weatherSystemState, m_sessionConfig.worldSeed, halfTurnStep(m_turnSystem), config);
    m_weatherMaskCache.clear();
    m_nextMapObjectId = 1;
    m_nextAutonomousUnitId = 1;

    m_eventLog.clear();
    m_eventLog.log(1, KingdomId::White,
        "Game started: " + m_sessionConfig.saveName + " [" + gameModeLabel(gameMode()) + "]");

    syncFactoryIds();
    return validate(errorMessage);
}

bool GameEngine::restoreFromSave(const SaveData& data,
                                 const GameConfig& config,
                                 std::string* errorMessage) {
    if (!GameStateValidator::validateSaveData(data, errorMessage)) {
        return false;
    }

    m_sessionConfig.saveName = data.gameName;
    m_sessionConfig.worldSeed = (data.worldSeed != 0) ? data.worldSeed : deriveLegacyWorldSeed(data);
    m_sessionConfig.kingdoms = data.sessionKingdoms;
    m_sessionConfig.multiplayer = data.multiplayer;

    m_board.init(data.mapRadius);
    if (!data.grid.empty()) {
        auto& grid = m_board.getGrid();
        const int diameter = m_board.getDiameter();
        for (int y = 0; y < diameter && y < static_cast<int>(data.grid.size()); ++y) {
            for (int x = 0; x < diameter && x < static_cast<int>(data.grid[y].size()); ++x) {
                grid[y][x].type = data.grid[y][x].type;
                grid[y][x].isInCircle = data.grid[y][x].isInCircle;
                grid[y][x].terrainBrightness = data.grid[y][x].terrainBrightness;
            }
        }

        BoardGenerator::applyTerrainVisuals(m_board, m_sessionConfig.worldSeed);
        for (int y = 0; y < diameter && y < static_cast<int>(data.grid.size()); ++y) {
            for (int x = 0; x < diameter && x < static_cast<int>(data.grid[y].size()); ++x) {
                if (data.grid[y][x].terrainFlipMask >= 0) {
                    grid[y][x].terrainFlipMask = data.grid[y][x].terrainFlipMask;
                }
                grid[y][x].terrainBrightness = data.grid[y][x].terrainBrightness;
            }
        }
    } else {
        std::vector<Building> generatedPublicBuildings;
        BoardGenerator::generate(m_board, config, generatedPublicBuildings, m_sessionConfig.worldSeed);
    }

    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        const auto kingdomId = static_cast<KingdomId>(kingdomSlot);
        m_kingdoms[kingdomSlot] = Kingdom(kingdomId);
        m_kingdoms[kingdomSlot].gold = data.kingdoms[kingdomSlot].gold;
        m_kingdoms[kingdomSlot].movementPointsMaxBonus = data.kingdoms[kingdomSlot].movementPointsMaxBonus;
        m_kingdoms[kingdomSlot].buildPointsMaxBonus = data.kingdoms[kingdomSlot].buildPointsMaxBonus;
        m_kingdoms[kingdomSlot].hasSpawnedBishop = data.kingdoms[kingdomSlot].hasSpawnedBishop;
        m_kingdoms[kingdomSlot].lastBishopSpawnParity = data.kingdoms[kingdomSlot].lastBishopSpawnParity & 1;
        for (const auto& piece : data.kingdoms[kingdomSlot].pieces) {
            m_kingdoms[kingdomSlot].addPiece(piece);
        }
        for (const auto& building : data.kingdoms[kingdomSlot].buildings) {
            m_kingdoms[kingdomSlot].addBuilding(building);
        }
        normalizeLoadedBuildings(
            m_kingdoms[kingdomSlot].buildings,
            m_sessionConfig.worldSeed,
            config);
    }

    m_publicBuildings = data.publicBuildings;
    m_mapObjects = data.mapObjects;
    m_autonomousUnits = data.autonomousUnits;
    m_chestSystemState = data.chestSystemState;
    m_weatherSystemState = data.weatherSystemState;
    m_weatherMaskCache = data.weatherMaskCache;
    m_xpSystemState = data.xpSystemState;
    m_infernalSystemState = data.infernalSystemState;
    normalizeLoadedBuildings(m_publicBuildings, m_sessionConfig.worldSeed, config);

    m_turnSystem = TurnSystem();
    m_turnSystem.setActiveKingdom(data.activeKingdom);
    m_turnSystem.setTurnNumber(data.turnNumber);
    if (!m_weatherSystemState.hasActiveFront
        && m_weatherSystemState.nextSpawnTurnStep <= 0
        && m_weatherSystemState.rngCounter == 0) {
        WeatherSystem::initialize(
            m_weatherSystemState,
            m_sessionConfig.worldSeed,
            halfTurnStep(m_turnSystem),
            config);
    } else if (m_weatherSystemState.revision == 0) {
        m_weatherSystemState.revision = 1;
    }
    const std::size_t expectedWeatherMaskCells = static_cast<std::size_t>(m_board.getDiameter() * m_board.getDiameter());
    if (m_weatherMaskCache.revision != m_weatherSystemState.revision
        || m_weatherMaskCache.diameter != m_board.getDiameter()
        || m_weatherMaskCache.alphaByCell.size() != expectedWeatherMaskCells
        || m_weatherMaskCache.shadeByCell.size() != expectedWeatherMaskCells) {
        m_weatherMaskCache.clear();
    }
    m_nextMapObjectId = 1;
    for (const MapObject& object : m_mapObjects) {
        m_nextMapObjectId = std::max(m_nextMapObjectId, object.id + 1);
    }
    m_nextAutonomousUnitId = 1;
    for (const AutonomousUnit& autonomousUnit : m_autonomousUnits) {
        m_nextAutonomousUnitId = std::max(m_nextAutonomousUnitId, autonomousUnit.id + 1);
    }

    m_eventLog.clear();
    for (const auto& event : data.events) {
        m_eventLog.log(event);
    }

    syncFactoryIds();
    return relinkAndValidateRuntimeState(errorMessage);
}

SaveData GameEngine::createSaveData() const {
    SaveData data;
    data.gameName = m_sessionConfig.saveName;
    data.turnNumber = m_turnSystem.getTurnNumber();
    data.activeKingdom = m_turnSystem.getActiveKingdom();
    data.mapRadius = m_board.getRadius();
    data.worldSeed = m_sessionConfig.worldSeed;
    data.sessionKingdoms = m_sessionConfig.kingdoms;
    data.multiplayer = m_sessionConfig.multiplayer;

    const int diameter = m_board.getDiameter();
    data.grid.resize(diameter);
    for (int y = 0; y < diameter; ++y) {
        data.grid[y].resize(diameter);
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = m_board.getCell(x, y);
            data.grid[y][x].type = cell.type;
            data.grid[y][x].isInCircle = cell.isInCircle;
            data.grid[y][x].terrainFlipMask = cell.terrainFlipMask;
            data.grid[y][x].terrainBrightness = cell.terrainBrightness;
        }
    }

    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        data.kingdoms[kingdomSlot].id = static_cast<KingdomId>(kingdomSlot);
        data.kingdoms[kingdomSlot].gold = m_kingdoms[kingdomSlot].gold;
        data.kingdoms[kingdomSlot].movementPointsMaxBonus = m_kingdoms[kingdomSlot].movementPointsMaxBonus;
        data.kingdoms[kingdomSlot].buildPointsMaxBonus = m_kingdoms[kingdomSlot].buildPointsMaxBonus;
        data.kingdoms[kingdomSlot].hasSpawnedBishop = m_kingdoms[kingdomSlot].hasSpawnedBishop;
        data.kingdoms[kingdomSlot].lastBishopSpawnParity = m_kingdoms[kingdomSlot].lastBishopSpawnParity;
        data.kingdoms[kingdomSlot].pieces.assign(m_kingdoms[kingdomSlot].pieces.begin(), m_kingdoms[kingdomSlot].pieces.end());
        data.kingdoms[kingdomSlot].buildings = m_kingdoms[kingdomSlot].buildings;
    }

    data.publicBuildings = m_publicBuildings;
    data.mapObjects = m_mapObjects;
    data.autonomousUnits = m_autonomousUnits;
    data.chestSystemState = m_chestSystemState;
    data.weatherSystemState = m_weatherSystemState;
    data.weatherMaskCache = m_weatherMaskCache;
    data.xpSystemState = m_xpSystemState;
    data.infernalSystemState = m_infernalSystemState;
    data.events = m_eventLog.getEvents();
    return data;
}

bool GameEngine::createValidatedSaveData(SaveData& outData,
                                         std::string* errorMessage) {
    std::string runtimeError;
    if (!relinkAndValidateRuntimeState(&runtimeError)) {
        if (errorMessage) {
            *errorMessage = runtimeError.empty()
                ? "Cannot serialize an invalid authoritative runtime state."
                : "Cannot serialize an invalid authoritative runtime state: " + runtimeError;
        }
        return false;
    }

    outData = createSaveData();

    std::string saveError;
    if (!GameStateValidator::validateSaveData(outData, &saveError)) {
        if (errorMessage) {
            *errorMessage = saveError.empty()
                ? "Cannot serialize invalid authoritative save data."
                : "Cannot serialize invalid authoritative save data: " + saveError;
        }
        return false;
    }

    return true;
}

void GameEngine::ensureWeatherMaskUpToDate(const GameConfig& config) {
    WeatherSystem::rebuildMask(m_board, m_weatherSystemState, config, m_weatherMaskCache);
}

void GameEngine::clearWeatherMaskCache() {
    m_weatherMaskCache.clear();
}

bool GameEngine::validate(std::string* errorMessage) const {
    return GameStateValidator::validateRuntimeState(
        m_board,
        m_kingdoms,
        m_publicBuildings,
        m_mapObjects,
        m_autonomousUnits,
        m_turnSystem,
        m_sessionConfig,
        m_xpSystemState,
        m_infernalSystemState,
        errorMessage);
}

void GameEngine::resetPendingTurn() {
    m_turnSystem.resetPendingCommands();
}

bool GameEngine::replacePendingCommands(const std::vector<TurnCommand>& commands,
                                        const GameConfig& config,
                                        bool assignAuthoritativeBuildIds,
                                        std::string* errorMessage) {
    resetPendingTurn();
    const TurnValidationContext turnContext = makeTurnValidationContext(config);
    for (const auto& submittedCommand : commands) {
        TurnCommand command = submittedCommand;
        if (assignAuthoritativeBuildIds && command.type == TurnCommand::Build) {
            command.buildId = -1;
        }

        if (!m_turnSystem.queueCommand(command, turnContext, &m_buildingFactory)) {
            resetPendingTurn();
            if (errorMessage) {
                *errorMessage = "The submitted turn contains an invalid or conflicting command.";
            }
            return false;
        }
    }

    return true;
}

bool GameEngine::triggerCheatcodeWeatherFront(const GameConfig& config) {
    const int currentTurnStep = halfTurnStep(m_turnSystem);
    if (WeatherSystem::hasActiveFront(m_weatherSystemState)) {
        WeatherSystem::clearActiveFront(m_weatherSystemState);
    }

    m_weatherSystemState.nextSpawnTurnStep = currentTurnStep;
    return WeatherSystem::trySpawnFront(
        m_weatherSystemState,
        m_board,
        m_sessionConfig.worldSeed,
        currentTurnStep,
        config);
}

bool GameEngine::triggerCheatcodeChestSpawn(const GameConfig& config) {
    if (m_chestSystemState.activeChestObjectId >= 0) {
        return false;
    }

    m_chestSystemState.nextSpawnTurn = m_turnSystem.getTurnNumber();
    return spawnChestIfDue(config);
}

bool GameEngine::triggerCheatcodeInfernalSpawn(const GameConfig& config) {
    if (InfernalSystem::hasActiveInfernal(m_infernalSystemState)) {
        return false;
    }

    const int currentTurnStep = halfTurnStep(m_turnSystem);
    if (std::optional<AutonomousUnit> spawnedInfernal = InfernalSystem::forceSpawnInfernal(
            m_infernalSystemState,
            m_board,
            m_kingdoms,
            m_weatherMaskCache,
            m_sessionConfig.worldSeed,
            currentTurnStep,
            m_turnSystem.getTurnNumber(),
            m_nextAutonomousUnitId,
            config)) {
        m_nextAutonomousUnitId = std::max(m_nextAutonomousUnitId, spawnedInfernal->id + 1);
        m_autonomousUnits.push_back(*spawnedInfernal);
        relinkRuntimeState();
        return true;
    }

    return false;
}

TurnValidationContext GameEngine::makeTurnValidationContext(const GameConfig& config) const {
    return TurnValidationContext{
        m_board,
        activeKingdom(),
        enemyKingdom(),
        m_publicBuildings,
        m_turnSystem.getTurnNumber(),
    config,
    m_sessionConfig.worldSeed,
    m_xpSystemState};
}

CheckTurnValidation GameEngine::validatePendingTurn(const GameConfig& config) const {
    return CheckResponseRules::validatePendingTurn(
        makeTurnValidationContext(config),
        m_turnSystem.getPendingCommands());
}

PendingTurnCommitResult GameEngine::commitPendingTurn(const GameConfig& config) {
    PendingTurnCommitResult result;
    const KingdomId activeId = m_turnSystem.getActiveKingdom();
    const KingdomId enemyId = opponent(activeId);
    const int activeInfernalBeforeCommit = m_infernalSystemState.activeInfernalUnitId;

    result.activeValidation = validatePendingTurn(config);
    if (!result.activeValidation.valid) {
        if (result.activeValidation.activeKingInCheck && !result.activeValidation.hasAnyLegalResponse) {
            result.gameOver = true;
            result.winner = enemyId;
        }
        return result;
    }

    m_turnSystem.commitTurn(m_board,
                            activeKingdom(),
                            enemyKingdom(),
                            m_publicBuildings,
                            m_mapObjects,
                            m_chestSystemState,
                            m_xpSystemState,
                            m_autonomousUnits,
                            m_infernalSystemState,
                            m_sessionConfig.worldSeed,
                            config,
                            m_eventLog,
                            result.notifications,
                            m_pieceFactory,
                            m_buildingFactory,
                            &m_weatherMaskCache);
    relinkRuntimeState();
    m_turnSystem.advanceTurn();
    const int currentHalfTurnStep = halfTurnStep(m_turnSystem);

    if (activeInfernalBeforeCommit >= 0 && !InfernalSystem::hasActiveInfernal(m_infernalSystemState)) {
        InfernalSystem::onInfernalRemoved(m_infernalSystemState, currentHalfTurnStep, config);
    }

    if (m_turnSystem.getActiveKingdom() == KingdomId::White) {
        InfernalSystem::decayBloodDebt(
            m_infernalSystemState,
            config.getInfernalBloodDebtDecayPercent());
    }

    const bool collectedChest = std::any_of(
        result.notifications.begin(),
        result.notifications.end(),
        [](const GameplayNotification& notification) {
            return notification.kind == GameplayNotificationKind::ChestReward;
        });
    if (collectedChest) {
        ChestSystem::scheduleNextSpawn(
            m_chestSystemState,
            m_sessionConfig.worldSeed,
            m_turnSystem.getTurnNumber(),
            config);
    }

    spawnChestIfDue(config);

        bool infernalBoardChanged = spawnInfernalIfDue(config, m_weatherMaskCache, currentHalfTurnStep);

    if (InfernalSystem::actAfterCommittedTurn(
            m_infernalSystemState,
            m_board,
            m_kingdoms,
            m_autonomousUnits,
            m_weatherMaskCache,
            m_sessionConfig.worldSeed,
            currentHalfTurnStep,
            m_turnSystem.getTurnNumber(),
            activeId,
            config)) {
        infernalBoardChanged = true;
    }

    WeatherSystem::advanceFront(
        m_weatherSystemState,
        m_sessionConfig.worldSeed,
        currentHalfTurnStep,
        config);
    WeatherSystem::trySpawnFront(
        m_weatherSystemState,
        m_board,
        m_sessionConfig.worldSeed,
        currentHalfTurnStep,
        config);

    if (infernalBoardChanged) {
        relinkRuntimeState();
    }

    result.committed = true;

    result.nextTurnValidation = validatePendingTurn(config);
    if (result.nextTurnValidation.activeKingInCheck && !result.nextTurnValidation.hasAnyLegalResponse) {
        result.gameOver = true;
        result.winner = activeId;
    }

    return result;
}

ControllerType GameEngine::controller(KingdomId id) const {
    return kingdomParticipantConfig(m_sessionConfig.kingdoms, id).controller;
}

bool GameEngine::isHumanControlled(KingdomId id) const {
    return controller(id) == ControllerType::Human;
}

bool GameEngine::isActiveHuman() const {
    return isHumanControlled(m_turnSystem.getActiveKingdom());
}

std::array<ControllerType, kNumKingdoms> GameEngine::controllers() const {
    return controllersFromParticipants(m_sessionConfig.kingdoms);
}

std::array<std::string, kNumKingdoms> GameEngine::participantNames() const {
    return participantNamesFromParticipants(m_sessionConfig.kingdoms);
}

std::string GameEngine::participantName(KingdomId id) const {
    const std::string& configuredName = kingdomParticipantConfig(m_sessionConfig.kingdoms, id).participantName;
    if (!configuredName.empty()) {
        return configuredName;
    }

    return (id == KingdomId::White) ? "White" : "Black";
}

std::string GameEngine::activeTurnLabel() const {
    const KingdomId activeId = m_turnSystem.getActiveKingdom();
    return participantName(activeId) + " - " + kingdomLabel(activeId)
        + " (Human)";
}

void GameEngine::syncFactoryIds() {
    m_pieceFactory.reset();
    m_buildingFactory.reset();

    for (const auto& kingdom : m_kingdoms) {
        for (const auto& piece : kingdom.pieces) {
            m_pieceFactory.observeExisting(piece.id);
        }
        for (const auto& building : kingdom.buildings) {
            m_buildingFactory.observeExisting(building.id);
        }
    }

    for (const auto& building : m_publicBuildings) {
        m_buildingFactory.observeExisting(building.id);
    }
}

bool GameEngine::spawnChestIfDue(const GameConfig& config) {
    std::optional<MapObject> spawnedChest = ChestSystem::trySpawnChest(
        m_chestSystemState,
        m_board,
        m_kingdoms,
        m_mapObjects,
        m_sessionConfig.worldSeed,
        m_turnSystem.getTurnNumber(),
        m_nextMapObjectId,
        config);
    if (!spawnedChest.has_value()) {
        relinkRuntimeState();
        return false;
    }

    m_nextMapObjectId = std::max(m_nextMapObjectId, spawnedChest->id + 1);
    m_mapObjects.push_back(*spawnedChest);
    relinkRuntimeState();
    m_eventLog.log(m_turnSystem.getTurnNumber(), m_turnSystem.getActiveKingdom(), "A chest appeared on the map.");
    return true;
}

bool GameEngine::spawnInfernalIfDue(const GameConfig& config,
                                    const WeatherMaskCache& weatherMaskCache,
                                    int currentTurnStep) {
    if (std::optional<AutonomousUnit> spawnedInfernal = InfernalSystem::trySpawnInfernal(
            m_infernalSystemState,
            m_board,
            m_kingdoms,
            weatherMaskCache,
            m_sessionConfig.worldSeed,
            currentTurnStep,
            m_turnSystem.getTurnNumber(),
            m_nextAutonomousUnitId,
            config)) {
        m_nextAutonomousUnitId = std::max(m_nextAutonomousUnitId, spawnedInfernal->id + 1);
        m_autonomousUnits.push_back(*spawnedInfernal);
        relinkRuntimeState();
        return true;
    }

    return false;
}