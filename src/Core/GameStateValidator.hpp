#pragma once

#include <array>
#include <string>
#include <vector>

#include "Autonomous/AutonomousUnit.hpp"
#include "Buildings/Building.hpp"
#include "Board/Board.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Objects/MapObject.hpp"
#include "Save/SaveData.hpp"
#include "Systems/InfernalSystem.hpp"
#include "Systems/TurnSystem.hpp"
#include "Systems/XPTypes.hpp"

class GameStateValidator {
public:
    static bool validateSessionConfig(const GameSessionConfig& session, std::string* errorMessage = nullptr);
    static bool validateSaveData(const SaveData& data, std::string* errorMessage = nullptr);
    static bool validateRuntimeState(const Board& board,
                                     const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                     const std::vector<Building>& publicBuildings,
                                     const std::vector<MapObject>& mapObjects,
                                     const std::vector<AutonomousUnit>& autonomousUnits,
                                     const TurnSystem& turnSystem,
                                     const GameSessionConfig& session,
                                     const XPSystemState& xpSystemState,
                                     const InfernalSystemState& infernalSystemState,
                                     std::string* errorMessage = nullptr);

private:
    static bool validateKingdomParticipants(const std::array<KingdomParticipantConfig, kNumKingdoms>& participants,
                                            std::string* errorMessage);
    static bool validateMultiplayerConfig(const std::array<KingdomParticipantConfig, kNumKingdoms>& participants,
                                          const MultiplayerConfig& multiplayer,
                                          std::string* errorMessage);
    static void writeError(std::string* errorMessage, const std::string& message);
};