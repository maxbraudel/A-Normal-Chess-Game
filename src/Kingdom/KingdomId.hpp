#pragma once

enum class KingdomId { White = 0, Black = 1 };

inline constexpr int kingdomIndex(KingdomId id) { return static_cast<int>(id); }

inline constexpr KingdomId opponent(KingdomId id) {
    return (id == KingdomId::White) ? KingdomId::Black : KingdomId::White;
}

inline constexpr int kNumKingdoms = 2;
