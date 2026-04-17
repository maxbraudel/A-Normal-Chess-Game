#pragma once

#include <array>
#include <string>
#include <vector>

#include "Autonomous/AutonomousUnit.hpp"
#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Objects/MapObject.hpp"
#include "Systems/XPTypes.hpp"

class GameConfig;
class GameEngine;
struct TurnCommand;

class TurnDraft {
public:
    bool rebuild(const GameEngine& engine,
                 const GameConfig& config,
                 const std::vector<TurnCommand>& commands,
                 std::string* errorMessage = nullptr);
    void clear();

    bool isValid() const { return m_valid; }
    const std::string& errorMessage() const { return m_errorMessage; }

    Board& board() { return m_board; }
    const Board& board() const { return m_board; }

    std::array<Kingdom, kNumKingdoms>& kingdoms() { return m_kingdoms; }
    const std::array<Kingdom, kNumKingdoms>& kingdoms() const { return m_kingdoms; }

    std::vector<Building>& publicBuildings() { return m_publicBuildings; }
    const std::vector<Building>& publicBuildings() const { return m_publicBuildings; }

    std::vector<MapObject>& mapObjects() { return m_mapObjects; }
    const std::vector<MapObject>& mapObjects() const { return m_mapObjects; }

    std::vector<AutonomousUnit>& autonomousUnits() { return m_autonomousUnits; }
    const std::vector<AutonomousUnit>& autonomousUnits() const { return m_autonomousUnits; }

    Kingdom& kingdom(KingdomId id) { return m_kingdoms[kingdomIndex(id)]; }
    const Kingdom& kingdom(KingdomId id) const { return m_kingdoms[kingdomIndex(id)]; }

private:
    bool applyMoveCommand(const TurnCommand& command,
                          KingdomId activeKingdomId,
                          const GameConfig& config);
    bool applyBuildCommand(const TurnCommand& command,
                           KingdomId activeKingdomId,
                           const GameConfig& config);
    void applyProduceReservation(const TurnCommand& command,
                                 KingdomId activeKingdomId,
                                 const GameConfig& config);
    void applyUpgradeReservation(const TurnCommand& command,
                                 KingdomId activeKingdomId,
                                 const GameConfig& config);
    bool applyDisbandCommand(const TurnCommand& command,
                             KingdomId activeKingdomId);
    void setError(const std::string& message, std::string* errorMessage);

    Board m_board;
    std::array<Kingdom, kNumKingdoms> m_kingdoms{Kingdom(KingdomId::White), Kingdom(KingdomId::Black)};
    std::vector<Building> m_publicBuildings;
    std::vector<MapObject> m_mapObjects;
    std::vector<AutonomousUnit> m_autonomousUnits;
    std::uint32_t m_worldSeed = 0;
    XPSystemState m_xpSystemState{};
    bool m_valid = false;
    std::string m_errorMessage;
};