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
#include "Systems/RewardProfileSampling.hpp"

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

void appendRewardAudit(XPSystemState& state,
                       XPRewardSource source,
                       const Piece& recipient,
                       int amount,
                       std::uint32_t rngCounterBefore,
                       bool hasVictimPieceType,
                       PieceType victimPieceType) {
    if (amount <= 0) {
        return;
    }

    XPRewardAuditEntry entry;
    entry.sequence = state.nextAuditSequence++;
    entry.source = source;
    entry.amount = amount;
    entry.recipientPieceId = recipient.id;
    entry.recipientPieceType = recipient.type;
    entry.recipientKingdom = recipient.kingdom;
    entry.recipientCellX = recipient.position.x;
    entry.recipientCellY = recipient.position.y;
    entry.recipientXpBefore = recipient.xp - amount;
    entry.recipientXpAfter = recipient.xp;
    entry.hasVictimPieceType = hasVictimPieceType;
    entry.victimPieceType = victimPieceType;
    entry.rngCounterBefore = rngCounterBefore;
    entry.rngCounterAfter = state.rngCounter;
    state.rewardAuditTrail.push_back(std::move(entry));
}

int sampleProfile(const XPRewardProfile& profile,
                  XPSystemState& state,
                  std::uint32_t worldSeed) {
    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    return RewardProfileSampling::sampleTruncatedNormal(profile, generator);
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
    const std::uint32_t rngCounterBefore = state.rngCounter;
    const int xp = sampleKillXP(victim, state, worldSeed, config);
    killer.xp += xp;
    appendRewardAudit(
        state,
        killSourceForVictim(victim),
        killer,
        xp,
        rngCounterBefore,
        true,
        victim);
    return xp;
}

int XPSystem::grantBlockDestroyXP(Piece& destroyer,
                                  XPSystemState& state,
                                  std::uint32_t worldSeed,
                                  const GameConfig& config) {
    const std::uint32_t rngCounterBefore = state.rngCounter;
    const int xp = sampleBlockDestroyXP(state, worldSeed, config);
    destroyer.xp += xp;
    appendRewardAudit(
        state,
        XPRewardSource::DestroyBlock,
        destroyer,
        xp,
        rngCounterBefore,
        false,
        PieceType::Pawn);
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
                    const std::uint32_t rngCounterBefore = state.rngCounter;
                    const int xp = sampleArenaXP(state, worldSeed, config);
                    p->xp += xp;
                    appendRewardAudit(
                        state,
                        XPRewardSource::ArenaPerTurn,
                        *p,
                        xp,
                        rngCounterBefore,
                        false,
                        PieceType::Pawn);
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
