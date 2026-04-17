#include "Systems/XPSystem.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include "Units/Piece.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
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

std::mt19937 makeEventGenerator(XPSystemState& state, std::uint32_t worldSeed) {
    const std::uint32_t baseSeed = (worldSeed == 0) ? 1u : worldSeed;
    return std::mt19937(mixSeed(baseSeed, state.rngCounter++));
}

XPRewardSource killSourceForVictim(PieceType victim) {
    switch (victim) {
        case PieceType::Pawn:
            return XPRewardSource::KillPawn;
        case PieceType::Knight:
            return XPRewardSource::KillKnight;
        case PieceType::Bishop:
            return XPRewardSource::KillBishop;
        case PieceType::Rook:
            return XPRewardSource::KillRook;
        case PieceType::Queen:
            return XPRewardSource::KillQueen;
        case PieceType::King:
        default:
            return XPRewardSource::DestroyBlock;
    }
}

int sampleProfile(const XPRewardProfile& profile,
                  XPSystemState& state,
                  std::uint32_t worldSeed) {
    const int minimum = std::max(0, profile.minimum);
    if (profile.mean <= 0) {
        return minimum;
    }

    const double mean = static_cast<double>(profile.mean);
    const double sigmaMultiplier = static_cast<double>(profile.sigmaMultiplierTimes100) / 100.0;
    const double clampMultiplier = static_cast<double>(profile.clampSigmaMultiplierTimes100) / 100.0;
    if (sigmaMultiplier <= 0.0 || clampMultiplier <= 0.0) {
        return std::max(minimum, profile.mean);
    }

    const double sigma = std::max(1.0, mean * sigmaMultiplier);
    const double delta = sigma * clampMultiplier;
    const double minValue = std::max(static_cast<double>(minimum), mean - delta);
    const double maxValue = std::max(minValue, mean + delta);

    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    std::normal_distribution<double> distribution(mean, sigma);
    const double clampedSample = std::clamp(distribution(generator), minValue, maxValue);
    return std::max(minimum, static_cast<int>(std::lround(clampedSample)));
}

}

int XPSystem::sampleReward(XPRewardSource source,
                           XPSystemState& state,
                           std::uint32_t worldSeed,
                           const GameConfig& config) {
    return sampleProfile(config.getXPRewardProfile(source), state, worldSeed);
}

int XPSystem::sampleKillXP(PieceType victim,
                           XPSystemState& state,
                           std::uint32_t worldSeed,
                           const GameConfig& config) {
    if (victim == PieceType::King) {
        return 0;
    }

    return sampleReward(killSourceForVictim(victim), state, worldSeed, config);
}

int XPSystem::sampleBlockDestroyXP(XPSystemState& state,
                                   std::uint32_t worldSeed,
                                   const GameConfig& config) {
    return sampleReward(XPRewardSource::DestroyBlock, state, worldSeed, config);
}

int XPSystem::sampleArenaXP(XPSystemState& state,
                            std::uint32_t worldSeed,
                            const GameConfig& config) {
    return sampleReward(XPRewardSource::ArenaPerTurn, state, worldSeed, config);
}

int XPSystem::grantKillXP(Piece& killer,
                          PieceType victim,
                          XPSystemState& state,
                          std::uint32_t worldSeed,
                          const GameConfig& config) {
    const int xp = sampleKillXP(victim, state, worldSeed, config);
    killer.xp += xp;
    return xp;
}

int XPSystem::grantBlockDestroyXP(Piece& destroyer,
                                  XPSystemState& state,
                                  std::uint32_t worldSeed,
                                  const GameConfig& config) {
    const int xp = sampleBlockDestroyXP(state, worldSeed, config);
    destroyer.xp += xp;
    return xp;
}

void XPSystem::grantArenaXP(Kingdom& kingdom,
                            const Board& board,
                            const std::vector<Building>& buildings,
                            XPSystemState& state,
                            std::uint32_t worldSeed,
                            const GameConfig& config) {
    for (const auto& b : buildings) {
        if (b.type != BuildingType::Arena) continue;
        if (b.owner != kingdom.id) continue;
        if (!b.isUsable()) continue;

        for (auto& pos : b.getOccupiedCells()) {
            const Cell& cell = board.getCell(pos.x, pos.y);
            if (cell.piece && cell.piece->kingdom == kingdom.id) {
                Piece* p = kingdom.getPieceById(cell.piece->id);
                if (p) {
                    p->xp += sampleArenaXP(state, worldSeed, config);
                }
            }
        }
    }
}

bool XPSystem::canUpgrade(const Piece& piece, PieceType target, const GameConfig& config) {
    return piece.canUpgradeTo(target, config);
}

void XPSystem::upgrade(Piece& piece, PieceType target) {
    piece.type = target;
}
