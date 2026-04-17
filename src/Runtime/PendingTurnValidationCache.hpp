#pragma once

#include <cstdint>
#include <deque>

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
            m_history.push_back(resolver());
            m_current = &m_history.back();
            m_key = key;
            m_valid = true;
        }

        return *m_current;
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
    std::deque<CheckTurnValidation> m_history;
    const CheckTurnValidation* m_current = nullptr;
};