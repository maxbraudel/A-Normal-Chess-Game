#pragma once

#include <cstdint>
#include <vector>

#include "Systems/XPTypes.hpp"
#include "Units/PieceType.hpp"

class Piece;
class Kingdom;
class Board;
class Building;
class GameConfig;

class XPSystem {
public:
    static int sampleReward(XPRewardSource source,
                            XPSystemState& state,
                            std::uint32_t worldSeed,
                            const GameConfig& config);
    static int sampleKillXP(PieceType victim,
                            XPSystemState& state,
                            std::uint32_t worldSeed,
                            const GameConfig& config);
    static int sampleBlockDestroyXP(XPSystemState& state,
                                    std::uint32_t worldSeed,
                                    const GameConfig& config);
    static int sampleArenaXP(XPSystemState& state,
                             std::uint32_t worldSeed,
                             const GameConfig& config);

    static int grantKillXP(Piece& killer,
                           PieceType victim,
                           XPSystemState& state,
                           std::uint32_t worldSeed,
                           const GameConfig& config);
    static int grantBlockDestroyXP(Piece& destroyer,
                                   XPSystemState& state,
                                   std::uint32_t worldSeed,
                                   const GameConfig& config);
    static void grantArenaXP(Kingdom& kingdom,
                             const Board& board,
                             const std::vector<Building>& buildings,
                             XPSystemState& state,
                             std::uint32_t worldSeed,
                             const GameConfig& config);
    static bool canUpgrade(const Piece& piece, PieceType target, const GameConfig& config);
    static void upgrade(Piece& piece, PieceType target);
};
