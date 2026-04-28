#pragma once

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "Kingdom/KingdomId.hpp"

enum class BehavioralTelemetryOrigin {
    LocalHost = 0,
    LocalClient,
    RemoteClientReported,
    HostObserved
};

enum class BehavioralTelemetryStage {
    Interaction = 0,
    CommandLifecycle,
    Preview,
    Submission,
    Validation,
    Commit,
    Persistence
};

inline const char* behavioralTelemetryOriginKey(BehavioralTelemetryOrigin origin) {
    switch (origin) {
        case BehavioralTelemetryOrigin::LocalHost: return "local_host";
        case BehavioralTelemetryOrigin::LocalClient: return "local_client";
        case BehavioralTelemetryOrigin::RemoteClientReported: return "remote_client_reported";
        case BehavioralTelemetryOrigin::HostObserved: return "host_observed";
    }
    return "unknown";
}

inline const char* behavioralTelemetryOriginLabel(BehavioralTelemetryOrigin origin) {
    switch (origin) {
        case BehavioralTelemetryOrigin::LocalHost: return "Local Host";
        case BehavioralTelemetryOrigin::LocalClient: return "Local Client";
        case BehavioralTelemetryOrigin::RemoteClientReported: return "Remote Client Reported";
        case BehavioralTelemetryOrigin::HostObserved: return "Host Observed";
    }
    return "Unknown";
}

inline const char* behavioralTelemetryStageKey(BehavioralTelemetryStage stage) {
    switch (stage) {
        case BehavioralTelemetryStage::Interaction: return "interaction";
        case BehavioralTelemetryStage::CommandLifecycle: return "command_lifecycle";
        case BehavioralTelemetryStage::Preview: return "preview";
        case BehavioralTelemetryStage::Submission: return "submission";
        case BehavioralTelemetryStage::Validation: return "validation";
        case BehavioralTelemetryStage::Commit: return "commit";
        case BehavioralTelemetryStage::Persistence: return "persistence";
    }
    return "unknown";
}

inline const char* behavioralTelemetryStageLabel(BehavioralTelemetryStage stage) {
    switch (stage) {
        case BehavioralTelemetryStage::Interaction: return "Interaction";
        case BehavioralTelemetryStage::CommandLifecycle: return "Command Lifecycle";
        case BehavioralTelemetryStage::Preview: return "Preview";
        case BehavioralTelemetryStage::Submission: return "Submission";
        case BehavioralTelemetryStage::Validation: return "Validation";
        case BehavioralTelemetryStage::Commit: return "Commit";
        case BehavioralTelemetryStage::Persistence: return "Persistence";
    }
    return "Unknown";
}

struct BehavioralTelemetryEventDetails {
    bool accepted = false;
    bool hasAccepted = false;
    bool hasCell = false;
    sf::Vector2i cell{0, 0};
    int pieceId = -1;
    int buildId = -1;
    int commandAuditSequence = -1;
    std::uint64_t pendingStateRevision = 0;
    std::string reason;
};

struct BehavioralTelemetryEvent {
    int sequence = 0;
    int turnNumber = 0;
    KingdomId activeKingdom = KingdomId::White;
    BehavioralTelemetryOrigin origin = BehavioralTelemetryOrigin::LocalHost;
    BehavioralTelemetryStage stage = BehavioralTelemetryStage::Interaction;
    std::string eventKey;
    std::string eventLabel;
    long long turnElapsedMs = 0;
    long long hostObservedAtUnixMs = 0;
    bool accepted = false;
    bool hasAccepted = false;
    bool hasCell = false;
    sf::Vector2i cell{0, 0};
    int pieceId = -1;
    int buildId = -1;
    int commandAuditSequence = -1;
    std::uint64_t pendingStateRevision = 0;
    std::string reason;
};

struct BehavioralPendingTurnTelemetry {
    int turnNumber = 0;
    KingdomId activeKingdom = KingdomId::White;
    std::uint64_t pendingStateRevision = 0;
    std::uint64_t telemetryRevision = 0;
    std::vector<BehavioralTelemetryEvent> interactionTimeline;
    std::vector<BehavioralTelemetryEvent> orchestrationEvents;
};
