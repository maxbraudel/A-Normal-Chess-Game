#pragma once

#include <algorithm>
#include <array>

#include "Core/GameSessionConfig.hpp"

enum class LocalSessionMode {
    LocalOnly,
    LanHost,
    LanClient
};

struct LocalPlayerContext {
    LocalSessionMode mode = LocalSessionMode::LocalOnly;
    std::array<bool, kNumKingdoms> localControl{false, false};
    KingdomId perspectiveKingdom = KingdomId::White;

    bool isNetworked() const {
        return mode != LocalSessionMode::LocalOnly;
    }

    bool isLocallyControlled(KingdomId kingdom) const {
        return localControl[kingdomIndex(kingdom)];
    }
};

inline LocalPlayerContext makeLocalPlayerContextForSession(const GameSessionConfig& session) {
    LocalPlayerContext context;

    if (session.multiplayer.enabled) {
        context.mode = LocalSessionMode::LanHost;
        context.localControl = {true, false};
        context.perspectiveKingdom = KingdomId::White;
        return context;
    }

    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        const KingdomId kingdomId = static_cast<KingdomId>(kingdomSlot);
        context.localControl[kingdomSlot] = controllerFor(session, kingdomId) == ControllerType::Human;
    }

    if (context.localControl[kingdomIndex(KingdomId::White)]) {
        context.perspectiveKingdom = KingdomId::White;
    } else if (context.localControl[kingdomIndex(KingdomId::Black)]) {
        context.perspectiveKingdom = KingdomId::Black;
    }

    return context;
}

inline LocalPlayerContext makeLanClientLocalPlayerContext() {
    LocalPlayerContext context;
    context.mode = LocalSessionMode::LanClient;
    context.localControl = {false, true};
    context.perspectiveKingdom = KingdomId::Black;
    return context;
}

inline int countLocallyControlledKingdoms(const LocalPlayerContext& context) {
    return static_cast<int>(std::count(context.localControl.begin(),
                                       context.localControl.end(),
                                       true));
}

inline bool hasSingleLocallyControlledKingdom(const LocalPlayerContext& context) {
    return countLocallyControlledKingdoms(context) == 1;
}