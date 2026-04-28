#include "Telemetry/BehavioralTelemetryCollector.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace {

std::string eventKeyForCommandAuditAction(TurnCommandAuditAction action) {
    switch (action) {
        case TurnCommandAuditAction::Queue: return "command_queued";
        case TurnCommandAuditAction::Replace: return "command_replaced";
        case TurnCommandAuditAction::Cancel: return "command_cancelled";
        case TurnCommandAuditAction::Reset: return "pending_commands_reset";
    }
    return "command_unknown";
}

std::string eventLabelForCommandAuditAction(TurnCommandAuditAction action) {
    switch (action) {
        case TurnCommandAuditAction::Queue: return "Command Queued";
        case TurnCommandAuditAction::Replace: return "Command Replaced";
        case TurnCommandAuditAction::Cancel: return "Command Cancelled";
        case TurnCommandAuditAction::Reset: return "Pending Commands Reset";
    }
    return "Command Unknown";
}

} // namespace

BehavioralTelemetryCollector::BehavioralTelemetryCollector() = default;

void BehavioralTelemetryCollector::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (!m_enabled) {
        reset();
    }
}

void BehavioralTelemetryCollector::reset() {
    m_pendingTurn = {};
    m_hasTurnStartTime = false;
    m_turnStartTime = {};
}

void BehavioralTelemetryCollector::restorePendingTurn(const BehavioralPendingTurnTelemetry& pendingTurn) {
    if (!m_enabled) {
        return;
    }

    m_pendingTurn = pendingTurn;
    m_hasTurnStartTime = true;
    m_turnStartTime = std::chrono::steady_clock::now();
}

void BehavioralTelemetryCollector::beginPendingTurn(int turnNumber,
                                                    KingdomId activeKingdom,
                                                    BehavioralTelemetryOrigin localOrigin) {
    if (!m_enabled) {
        return;
    }

    ensurePendingTurn(turnNumber, activeKingdom, localOrigin);
}

void BehavioralTelemetryCollector::setPendingStateRevision(std::uint64_t pendingStateRevision) {
    if (!m_enabled) {
        return;
    }

    if (m_pendingTurn.pendingStateRevision == pendingStateRevision) {
        return;
    }

    m_pendingTurn.pendingStateRevision = pendingStateRevision;
    incrementTelemetryRevision();
}

void BehavioralTelemetryCollector::recordInteraction(int turnNumber,
                                                     KingdomId activeKingdom,
                                                     BehavioralTelemetryOrigin origin,
                                                     BehavioralTelemetryStage stage,
                                                     const std::string& eventKey,
                                                     const std::string& eventLabel,
                                                     const BehavioralTelemetryEventDetails& details) {
    if (!m_enabled) {
        return;
    }

    ensurePendingTurn(turnNumber, activeKingdom, origin);
    m_pendingTurn.interactionTimeline.push_back(
        makeEvent(turnNumber, activeKingdom, origin, stage, eventKey, eventLabel, details, 0));
    incrementTelemetryRevision();
}

void BehavioralTelemetryCollector::recordOrchestration(int turnNumber,
                                                       KingdomId activeKingdom,
                                                       BehavioralTelemetryOrigin origin,
                                                       BehavioralTelemetryStage stage,
                                                       const std::string& eventKey,
                                                       const std::string& eventLabel,
                                                       const BehavioralTelemetryEventDetails& details,
                                                       long long hostObservedAtUnixMs) {
    if (!m_enabled) {
        return;
    }

    ensurePendingTurn(turnNumber, activeKingdom, origin);
    m_pendingTurn.orchestrationEvents.push_back(
        makeEvent(turnNumber,
                  activeKingdom,
                  origin,
                  stage,
                  eventKey,
                  eventLabel,
                  details,
                  hostObservedAtUnixMs));
    incrementTelemetryRevision();
}

void BehavioralTelemetryCollector::recordCommandAudit(int turnNumber,
                                                      KingdomId activeKingdom,
                                                      BehavioralTelemetryOrigin origin,
                                                      const TurnCommandAuditEntry& audit) {
    BehavioralTelemetryEventDetails details;
    details.hasAccepted = true;
    details.accepted = audit.accepted;
    details.commandAuditSequence = audit.sequence;
    details.reason = audit.reason;

    if (audit.hasCommand) {
        details.pendingStateRevision = m_pendingTurn.pendingStateRevision;
        switch (audit.command.type) {
            case TurnCommand::Move:
                details.pieceId = audit.command.pieceId;
                details.hasCell = true;
                details.cell = audit.command.destination;
                break;
            case TurnCommand::Build:
                details.buildId = audit.command.buildId;
                details.hasCell = true;
                details.cell = audit.command.buildOrigin;
                break;
            case TurnCommand::Upgrade:
                details.pieceId = audit.command.upgradePieceId;
                break;
            case TurnCommand::Produce:
                details.buildId = audit.command.barracksId;
                break;
            case TurnCommand::Marry:
            case TurnCommand::FormGroup:
            case TurnCommand::BreakGroup:
            case TurnCommand::Disband:
                details.pieceId = audit.command.pieceId;
                break;
        }
    }

    recordInteraction(turnNumber,
                      activeKingdom,
                      origin,
                      BehavioralTelemetryStage::CommandLifecycle,
                      eventKeyForCommandAuditAction(audit.action),
                      eventLabelForCommandAuditAction(audit.action),
                      details);
}

void BehavioralTelemetryCollector::replaceRemoteClientReportedPendingTurn(
    const BehavioralPendingTurnTelemetry& snapshot,
    long long hostObservedAtUnixMs) {
    if (!m_enabled) {
        return;
    }

    ensurePendingTurn(snapshot.turnNumber,
                      snapshot.activeKingdom,
                      BehavioralTelemetryOrigin::HostObserved);
    m_pendingTurn.pendingStateRevision = std::max(m_pendingTurn.pendingStateRevision,
                                                  snapshot.pendingStateRevision);

    auto stripRemoteClientReported = [](std::vector<BehavioralTelemetryEvent>& events) {
        events.erase(std::remove_if(events.begin(),
                                    events.end(),
                                    [](const BehavioralTelemetryEvent& event) {
                                        return event.origin == BehavioralTelemetryOrigin::RemoteClientReported;
                                    }),
                     events.end());
    };

    stripRemoteClientReported(m_pendingTurn.interactionTimeline);
    stripRemoteClientReported(m_pendingTurn.orchestrationEvents);

    const long long resolvedHostObservedAtUnixMs =
        (hostObservedAtUnixMs != 0) ? hostObservedAtUnixMs : currentUnixMs();

    auto appendSnapshot = [resolvedHostObservedAtUnixMs](std::vector<BehavioralTelemetryEvent>& destination,
                                                         const std::vector<BehavioralTelemetryEvent>& source) {
        for (BehavioralTelemetryEvent event : source) {
            event.origin = BehavioralTelemetryOrigin::RemoteClientReported;
            event.hostObservedAtUnixMs = resolvedHostObservedAtUnixMs;
            destination.push_back(std::move(event));
        }
    };

    appendSnapshot(m_pendingTurn.interactionTimeline, snapshot.interactionTimeline);
    appendSnapshot(m_pendingTurn.orchestrationEvents, snapshot.orchestrationEvents);
    incrementTelemetryRevision();
}

BehavioralPendingTurnTelemetry BehavioralTelemetryCollector::snapshotPendingTurn() const {
    return m_pendingTurn;
}

BehavioralPendingTurnTelemetry BehavioralTelemetryCollector::finalizeCommittedTurn() {
    BehavioralPendingTurnTelemetry snapshot = snapshotPendingTurn();
    reset();
    return snapshot;
}

BehavioralTelemetryEvent BehavioralTelemetryCollector::makeEvent(
    int turnNumber,
    KingdomId activeKingdom,
    BehavioralTelemetryOrigin origin,
    BehavioralTelemetryStage stage,
    const std::string& eventKey,
    const std::string& eventLabel,
    const BehavioralTelemetryEventDetails& details,
    long long hostObservedAtUnixMs) const {
    BehavioralTelemetryEvent event;
    event.sequence = static_cast<int>(m_pendingTurn.interactionTimeline.size()
                                      + m_pendingTurn.orchestrationEvents.size())
        + 1;
    event.turnNumber = turnNumber;
    event.activeKingdom = activeKingdom;
    event.origin = origin;
    event.stage = stage;
    event.eventKey = eventKey;
    event.eventLabel = eventLabel;
    event.turnElapsedMs = currentTurnElapsedMs();
    event.hostObservedAtUnixMs = (hostObservedAtUnixMs != 0) ? hostObservedAtUnixMs : currentUnixMs();
    event.accepted = details.accepted;
    event.hasAccepted = details.hasAccepted;
    event.hasCell = details.hasCell;
    event.cell = details.cell;
    event.pieceId = details.pieceId;
    event.buildId = details.buildId;
    event.commandAuditSequence = details.commandAuditSequence;
    event.pendingStateRevision = details.pendingStateRevision;
    event.reason = details.reason;
    return event;
}

void BehavioralTelemetryCollector::ensurePendingTurn(int turnNumber,
                                                     KingdomId activeKingdom,
                                                     BehavioralTelemetryOrigin localOrigin) {
    if (m_pendingTurn.turnNumber == turnNumber
        && m_pendingTurn.activeKingdom == activeKingdom
        && m_hasTurnStartTime) {
        return;
    }

    m_localOrigin = localOrigin;
    m_pendingTurn = {};
    m_pendingTurn.turnNumber = turnNumber;
    m_pendingTurn.activeKingdom = activeKingdom;
    m_pendingTurn.telemetryRevision = 1;
    m_hasTurnStartTime = true;
    m_turnStartTime = std::chrono::steady_clock::now();
}

void BehavioralTelemetryCollector::incrementTelemetryRevision() {
    if (!m_enabled) {
        return;
    }

    if (m_pendingTurn.telemetryRevision == 0) {
        m_pendingTurn.telemetryRevision = 1;
        return;
    }

    ++m_pendingTurn.telemetryRevision;
}

long long BehavioralTelemetryCollector::currentTurnElapsedMs() const {
    if (!m_hasTurnStartTime) {
        return 0;
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - m_turnStartTime)
        .count();
}

long long BehavioralTelemetryCollector::currentUnixMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
