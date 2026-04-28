#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "Systems/TurnCommand.hpp"
#include "Telemetry/BehavioralTelemetryTypes.hpp"

class BehavioralTelemetryCollector {
public:
    BehavioralTelemetryCollector();

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void reset();
    void restorePendingTurn(const BehavioralPendingTurnTelemetry& pendingTurn);

    void beginPendingTurn(int turnNumber,
                          KingdomId activeKingdom,
                          BehavioralTelemetryOrigin localOrigin);
    void setPendingStateRevision(std::uint64_t pendingStateRevision);

    void recordInteraction(int turnNumber,
                           KingdomId activeKingdom,
                           BehavioralTelemetryOrigin origin,
                           BehavioralTelemetryStage stage,
                           const std::string& eventKey,
                           const std::string& eventLabel,
                           const BehavioralTelemetryEventDetails& details = {});
    void recordOrchestration(int turnNumber,
                             KingdomId activeKingdom,
                             BehavioralTelemetryOrigin origin,
                             BehavioralTelemetryStage stage,
                             const std::string& eventKey,
                             const std::string& eventLabel,
                             const BehavioralTelemetryEventDetails& details = {},
                             long long hostObservedAtUnixMs = 0);
    void recordCommandAudit(int turnNumber,
                            KingdomId activeKingdom,
                            BehavioralTelemetryOrigin origin,
                            const TurnCommandAuditEntry& audit);

    void replaceRemoteClientReportedPendingTurn(const BehavioralPendingTurnTelemetry& snapshot,
                                                long long hostObservedAtUnixMs);

    BehavioralPendingTurnTelemetry snapshotPendingTurn() const;
    BehavioralPendingTurnTelemetry finalizeCommittedTurn();
    long long currentTurnElapsedMs() const;

    std::uint64_t telemetryRevision() const { return m_pendingTurn.telemetryRevision; }

private:
    BehavioralTelemetryEvent makeEvent(int turnNumber,
                                       KingdomId activeKingdom,
                                       BehavioralTelemetryOrigin origin,
                                       BehavioralTelemetryStage stage,
                                       const std::string& eventKey,
                                       const std::string& eventLabel,
                                       const BehavioralTelemetryEventDetails& details,
                                       long long hostObservedAtUnixMs) const;
    void ensurePendingTurn(int turnNumber,
                           KingdomId activeKingdom,
                           BehavioralTelemetryOrigin localOrigin);
    void incrementTelemetryRevision();

    static long long currentUnixMs();

    bool m_enabled = false;
    BehavioralTelemetryOrigin m_localOrigin = BehavioralTelemetryOrigin::LocalHost;
    BehavioralPendingTurnTelemetry m_pendingTurn;
    bool m_hasTurnStartTime = false;
    std::chrono::steady_clock::time_point m_turnStartTime{};
};
