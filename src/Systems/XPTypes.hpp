#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Kingdom/KingdomId.hpp"
#include "Units/PieceType.hpp"

enum class XPRewardSource {
    KillPawn = 0,
    KillKnight,
    KillBishop,
    KillRook,
    KillQueen,
    DestroyBlock,
    ArenaPerTurn,
    Count
};

constexpr std::size_t kNumXPRewardSources = static_cast<std::size_t>(XPRewardSource::Count);

constexpr std::size_t xpRewardSourceIndex(XPRewardSource source) {
    return static_cast<std::size_t>(source);
}

struct XPRewardProfile {
    int mean = 0;
    int sigmaMultiplierTimes100 = 0;
    int clampSigmaMultiplierTimes100 = 200;
    int minimum = 0;
};

struct XPRewardAuditEntry {
    int sequence = 0;
    XPRewardSource source = XPRewardSource::DestroyBlock;
    int amount = 0;
    int recipientPieceId = -1;
    PieceType recipientPieceType = PieceType::Pawn;
    KingdomId recipientKingdom = KingdomId::White;
    int recipientCellX = 0;
    int recipientCellY = 0;
    int recipientXpBefore = 0;
    int recipientXpAfter = 0;
    bool hasVictimPieceType = false;
    PieceType victimPieceType = PieceType::Pawn;
    std::uint32_t rngCounterBefore = 0;
    std::uint32_t rngCounterAfter = 0;
};

struct XPSystemState {
    std::uint32_t rngCounter = 0;
    int nextAuditSequence = 0;
    std::vector<XPRewardAuditEntry> rewardAuditTrail;
};