#include "Runtime/SessionFlow.hpp"

#include <algorithm>

#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Debug/GameStateDebugRecorder.hpp"
#include "Multiplayer/MultiplayerRuntime.hpp"
#include "Save/SaveData.hpp"
#include "Save/SaveManager.hpp"

namespace {

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
                         const GameConfig& config,
                         std::string savesDirectory)
    : m_engine(engine)
    , m_saveManager(saveManager)
    , m_multiplayer(multiplayer)
    , m_debugRecorder(debugRecorder)
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
    return true;
}

bool SessionFlow::saveAuthoritativeSession(bool allowSave,
                                           std::string* errorMessage) {
    if (!allowSave) {
        writeError(errorMessage, "Client-side multiplayer sessions cannot save authoritative game state.");
        return false;
    }

    const SaveData data = m_engine.createSaveData();
    if (!m_saveManager.save(buildSavePath(m_savesDirectory, m_engine.gameName()), data)) {
        writeError(errorMessage, "Failed to save game!");
        return false;
    }

    return true;
}