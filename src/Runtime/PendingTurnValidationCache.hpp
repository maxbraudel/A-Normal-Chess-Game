#pragma once

#include <cstdint>

#include "Kingdom/KingdomId.hpp"
#include "Systems/CheckResponseRules.hpp"

struct PendingTurnValidationCacheKey {
    std::uint64_t pendingStateRevision = 0;
    KingdomId activeKingdom = KingdomId::White;
    int turnNumber = 0;
};

class PendingTurnValidationCache {
public:
    template <typename Resolver>
    const CheckTurnValidation& resolve(const PendingTurnValidationCacheKey& key, Resolver&& resolver) {
        if (!m_valid || !sameKey(m_key, key)) {
            m_validation = resolver();
            m_key = key;
            m_valid = true;
        }

        return m_validation;
    }

    void invalidate() {
        m_valid = false;
    }

private:
    static bool sameKey(const PendingTurnValidationCacheKey& lhs,
                        const PendingTurnValidationCacheKey& rhs) {
        return lhs.pendingStateRevision == rhs.pendingStateRevision
            && lhs.activeKingdom == rhs.activeKingdom
            && lhs.turnNumber == rhs.turnNumber;
    }

    bool m_valid = false;
    PendingTurnValidationCacheKey m_key;
    CheckTurnValidation m_validation;
};