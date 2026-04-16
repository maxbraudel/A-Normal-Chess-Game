#pragma once

#include "Core/InteractionPermissions.hpp"
#include "Runtime/FrontendCoordinator.hpp"

class InteractivePermissionsCache {
public:
    template <typename Resolver>
    const InteractionPermissions& resolve(const FrontendRuntimeState& state, Resolver&& resolver) {
        if (!m_valid || !sameRuntimeState(m_state, state)) {
            m_permissions = resolver();
            m_state = state;
            m_valid = true;
        }

        return m_permissions;
    }

    void invalidate() {
        m_valid = false;
    }

private:
    static bool sameLocalPlayerContext(const LocalPlayerContext& lhs, const LocalPlayerContext& rhs) {
        return lhs.mode == rhs.mode
            && lhs.localControl == rhs.localControl
            && lhs.perspectiveKingdom == rhs.perspectiveKingdom;
    }

    static bool sameRuntimeState(const FrontendRuntimeState& lhs, const FrontendRuntimeState& rhs) {
        return lhs.gameState == rhs.gameState
            && sameLocalPlayerContext(lhs.localPlayerContext, rhs.localPlayerContext)
            && lhs.overlaysVisible == rhs.overlaysVisible
            && lhs.inGameMenuOpen == rhs.inGameMenuOpen
            && lhs.waitingForRemoteTurnResult == rhs.waitingForRemoteTurnResult
            && lhs.hostAuthenticated == rhs.hostAuthenticated
            && lhs.clientAuthenticated == rhs.clientAuthenticated
            && lhs.awaitingReconnect == rhs.awaitingReconnect
            && lhs.hasReconnectRequest == rhs.hasReconnectRequest
            && lhs.hostJoinHint == rhs.hostJoinHint
            && lhs.activeKingdom == rhs.activeKingdom;
    }

    bool m_valid = false;
    FrontendRuntimeState m_state;
    InteractionPermissions m_permissions;
};