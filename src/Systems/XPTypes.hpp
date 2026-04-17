#pragma once

#include <cstddef>
#include <cstdint>

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

struct XPSystemState {
    std::uint32_t rngCounter = 0;
};