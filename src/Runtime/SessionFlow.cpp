#include "Runtime/SessionFlow.hpp"

#include <algorithm>

#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Data/GameDataRecorder.hpp"
#include "Debug/GameStateDebugRecorder.hpp"
#include "Multiplayer/MultiplayerRuntime.hpp"
#include "Save/SaveData.hpp"
#include "Save/SaveManager.hpp"
#include "Telemetry/BehavioralTelemetryCollector.hpp"

namespace {

constexpr const char* kDataDirectory = "Data";

void writeError(std::string* errorMessage, const std::string& message) {
    if (errorMessage) {
        *errorMessage = message;
    }
}

std::string buildSavePath(const std::string& savesDirectory, const std::string& saveName) {
    return savesDirectory + "/" + saveName + ".json";
}

} // namespace

SessionFlow::SessionFlow(GameEngine& engine,
                         SaveManager& saveManager,
                         MultiplayerRuntime& multiplayer,
                         GameStateDebugRecorder& debugRecorder,
                         GameDataRecorder& dataRecorder,
                         BehavioralTelemetryCollector& behavioralTelemetry,
                         const GameConfig& config,
                         std::string savesDirectory)
    : m_engine(engine)
    , m_saveManager(saveManager)
    , m_multiplayer(multiplayer)
    , m_debugRecorder(debugRecorder)
    , m_dataRecorder(dataRecorder)
    , m_behavioralTelemetry(behavioralTelemetry)
    , m_config(config)
    , m_savesDirectory(std::move(savesDirectory)) {}

bool SessionFlow::startNewSession(const GameSessionConfig& session,
                                  std::string* errorMessage) {
    const auto existingSaves = m_saveManager.listSaves(m_savesDirectory);
    if (std::find(existingSaves.begin(), existingSaves.end(), session.saveName) != existingSaves.end()) {
        writeError(errorMessage, "A save with this name already exists.");
        return false;
    }

    if (!m_engine.startNewSession(session, m_config, errorMessage)) {
        return false;
    }

    if (!m_multiplayer.startHostIfNeeded(session, session.saveName, errorMessage)) {
        m_multiplayer.resetConnections();
        return false;
    }

    if (session.multiplayer.enabled) {
        m_engine.eventLog().log(
            m_engine.turnSystem().getTurnNumber(),
            KingdomId::White,
            "LAN server started on port " + std::to_string(session.multiplayer.port) + ".");
    }

    m_debugRecorder.reset();
    m_debugRecorder.recordSnapshot(m_engine.turnSystem().getTurnNumber(),
                                   m_engine.turnSystem().getActiveKingdom(),
                                   m_engine.kingdoms(),
                                   "initial_state_new_game");
    if (session.dataCollectionEnabled) {
        m_dataRecorder.beginNewSession(session, m_engine.createSaveData());
    } else {
        m_dataRecorder.reset();
    }

    const bool telemetryEnabled = session.dataCollectionEnabled
        && session.behavioralTelemetryEnabled;
    m_behavioralTelemetry.setEnabled(telemetryEnabled);
    if (telemetryEnabled) {
        m_behavioralTelemetry.beginPendingTurn(
            m_engine.turnSystem().getTurnNumber(),
            m_engine.turnSystem().getActiveKingdom(),
            BehavioralTelemetryOrigin::LocalHost);
        m_behavioralTelemetry.setPendingStateRevision(
            m_engine.turnSystem().getPendingStateRevision());
        m_dataRecorder.setPendingTurnTelemetry(m_behavioralTelemetry.snapshotPendingTurn());
    } else {
        m_behavioralTelemetry.reset();
        m_dataRecorder.setPendingTurnTelemetry(BehavioralPendingTurnTelemetry{});
    }
    return true;
}

bool SessionFlow::loadSession(const std::string& saveName,
                              std::string* errorMessage) {
    SaveData data;
    const std::string path = buildSavePath(m_savesDirectory, saveName);
    if (!m_saveManager.load(path, data)) {
        writeError(errorMessage, "Failed to load save: " + path);
        return false;
    }

    if (data.gameName.empty()) {
        data.gameName = saveName;
    }

    if (!m_engine.restoreFromSave(data, m_config, errorMessage)) {
        return false;
    }

    if (!m_multiplayer.startHostIfNeeded(m_engine.sessionConfig(),
                                         m_engine.sessionConfig().saveName,
                                         errorMessage)) {
        m_multiplayer.resetConnections();
        return false;
    }

    if (m_engine.sessionConfig().multiplayer.enabled) {
        m_engine.eventLog().log(
            m_engine.turnSystem().getTurnNumber(),
            KingdomId::White,
            "LAN server started on port "
                + std::to_string(m_engine.sessionConfig().multiplayer.port) + ".");
    }

    m_debugRecorder.reset();
    m_debugRecorder.recordSnapshot(m_engine.turnSystem().getTurnNumber(),
                                   m_engine.turnSystem().getActiveKingdom(),
                                   m_engine.kingdoms(),
                                   "initial_state_loaded_game");
    m_dataRecorder.resumeOrBootstrapFromSave(
        m_engine.sessionConfig(),
        m_engine.createSaveData(),
        GameDataRecorder::buildCompanionPath(kDataDirectory, saveName),
        m_saveManager);

    const bool telemetryEnabled = m_engine.sessionConfig().dataCollectionEnabled
        && m_engine.sessionConfig().behavioralTelemetryEnabled;
    m_behavioralTelemetry.setEnabled(telemetryEnabled);
    if (telemetryEnabled) {
        if (m_dataRecorder.pendingTurnTelemetry().turnNumber > 0) {
            m_behavioralTelemetry.restorePendingTurn(m_dataRecorder.pendingTurnTelemetry());
        } else {
            m_behavioralTelemetry.beginPendingTurn(
                m_engine.turnSystem().getTurnNumber(),
                m_engine.turnSystem().getActiveKingdom(),
                BehavioralTelemetryOrigin::LocalHost);
        }
        m_behavioralTelemetry.setPendingStateRevision(
            m_engine.turnSystem().getPendingStateRevision());
        m_dataRecorder.setPendingTurnTelemetry(m_behavioralTelemetry.snapshotPendingTurn());
    } else {
        m_behavioralTelemetry.reset();
        m_dataRecorder.setPendingTurnTelemetry(BehavioralPendingTurnTelemetry{});
    }
    return true;
}

bool SessionFlow::saveAuthoritativeSession(bool allowSave,
                                           std::string* errorMessage) {
    if (!allowSave) {
        writeError(errorMessage, "Client-side multiplayer sessions cannot save authoritative game state.");
        return false;
    }

    m_engine.ensureWeatherMaskUpToDate(m_config);
    SaveData data;
    if (!m_engine.createValidatedSaveData(data, errorMessage)) {
        return false;
    }

    if (!m_saveManager.save(buildSavePath(m_savesDirectory, m_engine.gameName()), data)) {
        writeError(errorMessage, "Failed to save game!");
        return false;
    }

    if (m_engine.sessionConfig().dataCollectionEnabled
        && m_engine.sessionConfig().behavioralTelemetryEnabled) {
        m_dataRecorder.setPendingTurnTelemetry(m_behavioralTelemetry.snapshotPendingTurn());
    } else if (m_engine.sessionConfig().dataCollectionEnabled) {
        m_dataRecorder.setPendingTurnTelemetry(BehavioralPendingTurnTelemetry{});
    }

    if (m_engine.sessionConfig().dataCollectionEnabled
        && !m_dataRecorder.saveToFile(
            GameDataRecorder::buildCompanionPath(kDataDirectory, m_engine.gameName()),
            m_config,
            m_saveManager,
            errorMessage)) {
        return false;
    }

    return true;
}