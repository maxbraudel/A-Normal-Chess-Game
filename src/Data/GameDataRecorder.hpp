#pragma once

#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

#include "Core/GameSessionConfig.hpp"
#include "Core/GameplayNotification.hpp"
#include "Save/SaveData.hpp"
#include "Systems/CheckResponseRules.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/TurnCommand.hpp"

class SaveManager;

struct GameDataTurnRecord {
    int committedTurnNumber = 0;
    KingdomId committedActiveKingdom = KingdomId::White;
    bool gameOver = false;
    KingdomId winner = KingdomId::White;
    std::time_t capturedAtUnix = 0;
    CheckTurnValidation activeValidation;
    CheckTurnValidation nextTurnValidation;
    std::vector<TurnCommand> queuedCommands;
    std::vector<GameplayNotification> notifications;
    std::vector<EventLog::Event> newEvents;
    SaveData snapshot;
};

class GameDataRecorder {
public:
    static constexpr int kSchemaVersion = 2;

    void reset();
    void beginNewSession(const GameSessionConfig& session,
                         const SaveData& initialSnapshot);
    bool resumeOrBootstrapFromSave(const GameSessionConfig& session,
                                   const SaveData& currentSnapshot,
                                   const std::string& dataFilePath,
                                   SaveManager& saveManager);
    void recordCommittedTurn(const std::vector<TurnCommand>& queuedCommands,
                             int committedTurnNumber,
                             KingdomId committedActiveKingdom,
                             const CheckTurnValidation& activeValidation,
                             const CheckTurnValidation& nextTurnValidation,
                             bool gameOver,
                             KingdomId winner,
                             const std::vector<GameplayNotification>& notifications,
                             const SaveData& snapshot);
    bool saveToFile(const std::string& dataFilePath,
                    const GameConfig& config,
                    SaveManager& saveManager,
                    std::string* errorMessage = nullptr);

    bool isEnabled() const { return m_enabled; }

    static std::string buildCompanionPath(const std::string& dataDirectory,
                                          const std::string& saveName);
    static bool deleteCompanion(const std::string& dataDirectory,
                                const std::string& saveName);
    static bool renameCompanion(const std::string& dataDirectory,
                                const std::string& oldSaveName,
                                const std::string& newSaveName);

private:
    void bootstrapFromSnapshot(const GameSessionConfig& session,
                               const SaveData& snapshot,
                               bool historyContinuityComplete,
                               const std::string& initialSnapshotReason);
    bool loadFromFile(const std::string& dataFilePath,
                      SaveManager& saveManager);
    const SaveData& currentSnapshot() const;

    bool m_enabled = false;
    bool m_historyContinuityComplete = true;
    bool m_loadedFromExistingCompanion = false;
    std::string m_saveName;
    std::time_t m_createdAtUnix = 0;
    std::time_t m_lastUpdatedAtUnix = 0;
    std::string m_initialSnapshotReason;
    SaveData m_initialSnapshot;
    std::vector<GameDataTurnRecord> m_turnHistory;
    std::size_t m_lastRecordedEventCount = 0;
};