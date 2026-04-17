#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "Core/GameplayNotification.hpp"
#include "Objects/MapObject.hpp"

class Board;
class GameConfig;
class Kingdom;

struct ChestSystemState {
    int activeChestObjectId = -1;
    int nextSpawnTurn = 0;
    std::uint32_t rngCounter = 0;
};

struct ChestClaimResult {
    int objectId = -1;
    ChestReward reward{};
    GameplayNotification notification{};
};

class ChestSystem {
public:
    static void initialize(ChestSystemState& state,
                           std::uint32_t worldSeed,
                           int currentTurn,
                           const GameConfig& config);
    static void scheduleNextSpawn(ChestSystemState& state,
                                  std::uint32_t worldSeed,
                                  int currentTurn,
                                  const GameConfig& config);
    static std::optional<MapObject> trySpawnChest(ChestSystemState& state,
                                                  const Board& board,
                                                  const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                                  const std::vector<MapObject>& mapObjects,
                                                  std::uint32_t worldSeed,
                                                  int currentTurn,
                                                  int nextObjectId,
                                                  const GameConfig& config);
    static std::optional<ChestClaimResult> collectChestAtPosition(std::vector<MapObject>& mapObjects,
                                                                  ChestSystemState& state,
                                                                  sf::Vector2i position,
                                                                  Kingdom& collector);
};