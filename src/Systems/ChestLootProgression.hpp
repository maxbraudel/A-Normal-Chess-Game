#pragma once

#include <array>
#include <cstdint>

#include "Kingdom/KingdomId.hpp"
#include "Objects/MapObject.hpp"

class GameConfig;

struct ChestLootProgressionState {
    bool hasCurrentReward = false;
    int currentRewardGeneration = 0;
    ChestReward currentReward{};
    std::array<int, kNumKingdoms> lastCollectedGenerationByKingdom{};
};

struct ChestLootResolution {
    ChestReward reward{};
    bool generatedNewReward = false;
    int generation = 0;
};

class ChestLootProgression {
public:
    static void reset(ChestLootProgressionState& state);
    static ChestLootResolution resolveReward(ChestLootProgressionState& state,
                                             std::uint32_t& rewardRngCounter,
                                             KingdomId collector,
                                             std::uint32_t worldSeed,
                                             int currentTurn,
                                             const GameConfig& config);
};