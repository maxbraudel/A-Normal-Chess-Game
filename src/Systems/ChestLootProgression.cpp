#include "Systems/ChestLootProgression.hpp"

#include <algorithm>
#include <array>
#include <numeric>
#include <random>

#include "Config/GameConfig.hpp"

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

std::mt19937 makeRewardGenerator(std::uint32_t& rewardRngCounter, std::uint32_t worldSeed) {
    const std::uint32_t baseSeed = (worldSeed == 0) ? 1u : worldSeed;
    return std::mt19937(mixSeed(baseSeed, rewardRngCounter++));
}

ChestReward sampleReward(std::uint32_t& rewardRngCounter,
                         std::uint32_t worldSeed,
                         int currentTurn,
                         const GameConfig& config) {
    const bool lateGame = currentTurn >= config.getChestLateGameTurn();
    const std::array<int, 3> weights{
        lateGame ? config.getChestLateGoldWeight() : config.getChestEarlyGoldWeight(),
        lateGame ? config.getChestLateMovementBonusWeight() : config.getChestEarlyMovementBonusWeight(),
        lateGame ? config.getChestLateBuildBonusWeight() : config.getChestEarlyBuildBonusWeight()};

    std::mt19937 generator = makeRewardGenerator(rewardRngCounter, worldSeed);
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

} // namespace

void ChestLootProgression::reset(ChestLootProgressionState& state) {
    state = ChestLootProgressionState{};
}

ChestLootResolution ChestLootProgression::resolveReward(ChestLootProgressionState& state,
                                                        std::uint32_t& rewardRngCounter,
                                                        KingdomId collector,
                                                        std::uint32_t worldSeed,
                                                        int currentTurn,
                                                        const GameConfig& config) {
    ChestLootResolution resolution;
    if (!config.isChestCurrentLootCatchUpEnabled()) {
        resolution.reward = sampleReward(rewardRngCounter, worldSeed, currentTurn, config);
        resolution.generatedNewReward = true;
        return resolution;
    }

    const int kingdomSlot = kingdomIndex(collector);
    const int lastCollectedGeneration = state.lastCollectedGenerationByKingdom[kingdomSlot];
    if (!state.hasCurrentReward || lastCollectedGeneration >= state.currentRewardGeneration) {
        state.currentRewardGeneration = std::max(1, state.currentRewardGeneration + 1);
        state.currentReward = sampleReward(rewardRngCounter, worldSeed, currentTurn, config);
        state.hasCurrentReward = true;
        resolution.generatedNewReward = true;
    }

    state.lastCollectedGenerationByKingdom[kingdomSlot] = state.currentRewardGeneration;
    resolution.reward = state.currentReward;
    resolution.generation = state.currentRewardGeneration;
    return resolution;
}