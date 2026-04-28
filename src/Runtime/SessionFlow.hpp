#pragma once

#include <string>

#include "Core/GameSessionConfig.hpp"

class GameConfig;
class GameDataRecorder;
class GameEngine;
class GameStateDebugRecorder;
class MultiplayerRuntime;
class SaveManager;

class SessionFlow {
public:
    SessionFlow(GameEngine& engine,
                SaveManager& saveManager,
                MultiplayerRuntime& multiplayer,
                GameStateDebugRecorder& debugRecorder,
                GameDataRecorder& dataRecorder,
                const GameConfig& config,
                std::string savesDirectory = "saves");

    bool startNewSession(const GameSessionConfig& session,
                         std::string* errorMessage = nullptr);
    bool loadSession(const std::string& saveName,
                     std::string* errorMessage = nullptr);
    bool saveAuthoritativeSession(bool allowSave,
                                  std::string* errorMessage = nullptr);

private:
    GameEngine& m_engine;
    SaveManager& m_saveManager;
    MultiplayerRuntime& m_multiplayer;
    GameStateDebugRecorder& m_debugRecorder;
    GameDataRecorder& m_dataRecorder;
    const GameConfig& m_config;
    std::string m_savesDirectory;
};