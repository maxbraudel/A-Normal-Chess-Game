#include "Systems/ChestSystem.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Board/CellType.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"

namespace {

std::uint32_t mixSeed(std::uint32_t seed, std::uint32_t value) {
    std::uint32_t mixed = seed ^ (value + 0x9e3779b9u + (seed << 6) + (seed >> 2));
    mixed ^= mixed >> 16;
    mixed *= 0x7feb352du;
    mixed ^= mixed >> 15;
    mixed *= 0x846ca68bu;
    mixed ^= mixed >> 16;
    return mixed;
}

std::mt19937 makeEventGenerator(ChestSystemState& state, std::uint32_t worldSeed) {
    const std::uint32_t baseSeed = (worldSeed == 0) ? 1u : worldSeed;
    return std::mt19937(mixSeed(baseSeed, state.rngCounter++));
}

int manhattanDistance(sf::Vector2i lhs, sf::Vector2i rhs) {
    return std::abs(lhs.x - rhs.x) + std::abs(lhs.y - rhs.y);
}

int sampleSpawnDelay(ChestSystemState& state,
                     std::uint32_t worldSeed,
                     const GameConfig& config) {
    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    const double shape = std::max(0.10, static_cast<double>(config.getChestWeibullShapeTimes100()) / 100.0);
    const double scale = std::max(0.50, static_cast<double>(config.getChestWeibullScaleTurns()));
    std::weibull_distribution<double> distribution(shape, scale);
    const int randomDelay = std::max(0, static_cast<int>(std::lround(distribution(generator))));
    return std::max(config.getChestRespawnCooldownTurns(), randomDelay);
}

ChestReward sampleReward(ChestSystemState& state,
                         std::uint32_t worldSeed,
                         int currentTurn,
                         const GameConfig& config) {
    const bool lateGame = currentTurn >= config.getChestLateGameTurn();
    const std::array<int, 3> weights{
        lateGame ? config.getChestLateGoldWeight() : config.getChestEarlyGoldWeight(),
        lateGame ? config.getChestLateMovementBonusWeight() : config.getChestEarlyMovementBonusWeight(),
        lateGame ? config.getChestLateBuildBonusWeight() : config.getChestEarlyBuildBonusWeight()};

    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    const int totalWeight = std::accumulate(weights.begin(), weights.end(), 0);
    if (totalWeight <= 0) {
        return ChestReward{ChestRewardType::Gold, config.getChestGoldRewardAmount()};
    }

    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    switch (distribution(generator)) {
        case 1:
            return ChestReward{ChestRewardType::MovementPointsMaxBonus, config.getChestMovementBonusAmount()};
        case 2:
            return ChestReward{ChestRewardType::BuildPointsMaxBonus, config.getChestBuildBonusAmount()};
        case 0:
        default:
            return ChestReward{ChestRewardType::Gold, config.getChestGoldRewardAmount()};
    }
}

int spawnWeightForCell(const Board& board,
                      const std::array<Kingdom, kNumKingdoms>& kingdoms,
                      sf::Vector2i cellPos,
                      int minDistanceFromKings) {
    const Piece* whiteKing = kingdoms[kingdomIndex(KingdomId::White)].getKing();
    const Piece* blackKing = kingdoms[kingdomIndex(KingdomId::Black)].getKing();
    if (!whiteKing || !blackKing) {
        return 0;
    }

    const int distWhite = manhattanDistance(cellPos, whiteKing->position);
    const int distBlack = manhattanDistance(cellPos, blackKing->position);
    if (std::min(distWhite, distBlack) < minDistanceFromKings) {
        return 0;
    }

    const sf::Vector2i center{board.getRadius(), board.getRadius()};
    const int distCenter = manhattanDistance(cellPos, center);
    const int centrality = std::max(0, board.getRadius() * 2 - distCenter);
    const int contestation = std::max(0, board.getRadius() * 2 - std::abs(distWhite - distBlack));
    return 1 + centrality + contestation;
}

GameplayNotification buildChestNotification(KingdomId collector, const ChestReward& reward) {
    GameplayNotification notification;
    notification.kind = GameplayNotificationKind::ChestReward;
    notification.kingdom = collector;
    notification.chestReward = reward;
    return notification;
}

} // namespace

void ChestSystem::initialize(ChestSystemState& state,
                             std::uint32_t worldSeed,
                             int currentTurn,
                             const GameConfig& config) {
    state.activeChestObjectId = -1;
    state.nextSpawnTurn = 0;
    state.rngCounter = 0;
    scheduleNextSpawn(state, worldSeed, std::max(1, currentTurn), config);
    state.nextSpawnTurn = std::max(state.nextSpawnTurn, config.getChestMinSpawnTurn());
}

void ChestSystem::scheduleNextSpawn(ChestSystemState& state,
                                    std::uint32_t worldSeed,
                                    int currentTurn,
                                    const GameConfig& config) {
    state.activeChestObjectId = -1;
    state.nextSpawnTurn = std::max(currentTurn + 1,
                                   currentTurn + sampleSpawnDelay(state, worldSeed, config));
}

std::optional<MapObject> ChestSystem::trySpawnChest(ChestSystemState& state,
                                                    const Board& board,
                                                    const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                                    const std::vector<MapObject>& mapObjects,
                                                    std::uint32_t worldSeed,
                                                    int currentTurn,
                                                    int nextObjectId,
                                                    const GameConfig& config) {
    (void) mapObjects;
    if (state.activeChestObjectId >= 0 || currentTurn < state.nextSpawnTurn) {
        return std::nullopt;
    }

    struct SpawnCandidate {
        sf::Vector2i position{0, 0};
        int weight = 0;
    };

    std::vector<SpawnCandidate> candidates;
    candidates.reserve(board.getDiameter() * board.getDiameter());

    for (const sf::Vector2i& cellPos : board.getAllValidCells()) {
        const Cell& cell = board.getCell(cellPos.x, cellPos.y);
        if (cell.type == CellType::Water || cell.building != nullptr || cell.piece != nullptr || cell.mapObject != nullptr) {
            continue;
        }

        const int weight = spawnWeightForCell(board,
                                              kingdoms,
                                              cellPos,
                                              config.getChestMinDistanceFromKings());
        if (weight <= 0) {
            continue;
        }

        candidates.push_back(SpawnCandidate{cellPos, weight});
    }

    if (candidates.empty()) {
        state.nextSpawnTurn = currentTurn + std::max(1, config.getChestSpawnRetryTurns());
        return std::nullopt;
    }

    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    std::vector<int> weights;
    weights.reserve(candidates.size());
    for (const SpawnCandidate& candidate : candidates) {
        weights.push_back(candidate.weight);
    }

    std::discrete_distribution<std::size_t> distribution(weights.begin(), weights.end());
    const SpawnCandidate& chosen = candidates[distribution(generator)];

    MapObject object;
    object.id = nextObjectId;
    object.type = MapObjectType::Chest;
    object.position = chosen.position;
    object.chest.reward = sampleReward(state, worldSeed, currentTurn, config);
    object.chest.spawnTurn = currentTurn;
    state.activeChestObjectId = object.id;
    return object;
}

std::optional<ChestClaimResult> ChestSystem::collectChestAtPosition(std::vector<MapObject>& mapObjects,
                                                                    ChestSystemState& state,
                                                                    sf::Vector2i position,
                                                                    Kingdom& collector) {
    const auto it = std::find_if(mapObjects.begin(), mapObjects.end(), [&](const MapObject& object) {
        return object.position == position
            && object.type == MapObjectType::Chest
            && (state.activeChestObjectId < 0 || object.id == state.activeChestObjectId);
    });
    if (it == mapObjects.end()) {
        return std::nullopt;
    }

    ChestClaimResult result;
    result.objectId = it->id;
    result.reward = it->chest.reward;

    switch (result.reward.type) {
        case ChestRewardType::Gold:
            collector.gold += result.reward.amount;
            break;
        case ChestRewardType::MovementPointsMaxBonus:
            collector.movementPointsMaxBonus += result.reward.amount;
            break;
        case ChestRewardType::BuildPointsMaxBonus:
            collector.buildPointsMaxBonus += result.reward.amount;
            break;
    }

    result.notification = buildChestNotification(collector.id, result.reward);
    mapObjects.erase(it);
    state.activeChestObjectId = -1;
    return result;
}