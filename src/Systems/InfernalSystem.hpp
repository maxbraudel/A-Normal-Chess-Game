#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "Autonomous/AutonomousUnit.hpp"

class Board;
class GameConfig;
class Kingdom;

struct InfernalSystemState {
    int activeInfernalUnitId = -1;
    int nextSpawnTurn = 0;
    int whiteBloodDebt = 0;
    int blackBloodDebt = 0;
    std::uint32_t rngCounter = 0;
};

class InfernalSystem {
public:
    static void initialize(InfernalSystemState& state,
                           int currentTurnStep,
                           const GameConfig& config);
    static void scheduleNextSpawn(InfernalSystemState& state,
                                  int currentTurnStep,
                                  const GameConfig& config);
    static void onInfernalRemoved(InfernalSystemState& state,
                                  int currentTurnStep,
                                  const GameConfig& config);
    static void decayBloodDebt(InfernalSystemState& state, int decayPercent);
    static void addBloodDebtForCapturedPiece(InfernalSystemState& state,
                                             KingdomId actorKingdom,
                                             PieceType victimType,
                                             int debtAmount);
    static void addBloodDebtForStructureDamage(InfernalSystemState& state,
                                               KingdomId actorKingdom,
                                               int debtAmount);
    static bool hasActiveInfernal(const InfernalSystemState& state);
    static AutonomousUnit* findActiveInfernal(std::vector<AutonomousUnit>& units,
                                              const InfernalSystemState& state);
    static const AutonomousUnit* findActiveInfernal(const std::vector<AutonomousUnit>& units,
                                                    const InfernalSystemState& state);
    static void clearActiveInfernal(InfernalSystemState& state);
    static std::optional<AutonomousUnit> trySpawnInfernal(
        InfernalSystemState& state,
        const Board& board,
        const std::array<Kingdom, kNumKingdoms>& kingdoms,
        std::uint32_t worldSeed,
        int currentTurnStep,
        int currentTurnNumber,
        int nextUnitId,
        const GameConfig& config);
    static std::optional<AutonomousUnit> forceSpawnInfernal(
        InfernalSystemState& state,
        const Board& board,
        const std::array<Kingdom, kNumKingdoms>& kingdoms,
        std::uint32_t worldSeed,
        int currentTurnStep,
        int currentTurnNumber,
        int nextUnitId,
        const GameConfig& config);
    static bool actAfterCommittedTurn(InfernalSystemState& state,
                                      Board& board,
                                      std::array<Kingdom, kNumKingdoms>& kingdoms,
                                      std::vector<AutonomousUnit>& units,
                                      std::uint32_t worldSeed,
                                      int currentTurnStep,
                                      int currentTurnNumber,
                                      KingdomId justEndedKingdom,
                                      const GameConfig& config);
    static bool shouldActAfterCommittedTurn(KingdomId targetKingdom, KingdomId justEndedKingdom);
};