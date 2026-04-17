#pragma once

#include <array>
#include <cstdint>

#include "Systems/WeatherTypes.hpp"

class Board;
class GameConfig;

class WeatherSystem {
public:
    static constexpr int kStepsPerTurn = 2;

    static void initialize(WeatherSystemState& state,
                           std::uint32_t worldSeed,
                           int currentTurnStep,
                           const GameConfig& config);
    static bool hasActiveFront(const WeatherSystemState& state);
    static void clearActiveFront(WeatherSystemState& state);
    static void scheduleNextSpawn(WeatherSystemState& state,
                                  std::uint32_t worldSeed,
                                  int currentTurnStep,
                                  const GameConfig& config);
    static bool trySpawnFront(WeatherSystemState& state,
                              const Board& board,
                              std::uint32_t worldSeed,
                              int currentTurnStep,
                              const GameConfig& config);
    static bool advanceFront(WeatherSystemState& state,
                             std::uint32_t worldSeed,
                             int currentTurnStep,
                             const GameConfig& config);

    static void rebuildMask(const Board& board,
                            const WeatherSystemState& state,
                            const GameConfig& config,
                            WeatherMaskCache& cache);

    static std::uint8_t alphaAtCell(const WeatherMaskCache& cache, int x, int y);
    static std::uint8_t shadeAtCell(const WeatherMaskCache& cache, int x, int y);
    static std::array<int, 2> directionStep(WeatherDirection direction);
};