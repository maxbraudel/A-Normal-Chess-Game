#pragma once

#include <cstdint>

#include <SFML/System/Vector2.hpp>

#include "Systems/XPTypes.hpp"
#include "Units/PieceType.hpp"

class Piece;
class Board;
class Kingdom;
class GameConfig;
class EventLog;

class CombatSystem {
public:
    struct CombatResult {
        bool occurred;
        bool targetWasPiece;
        int xpGained;
        PieceType capturedPieceType;
    };

    static CombatResult resolve(
        Piece& attacker, Board& board, sf::Vector2i target,
        Kingdom& attackerKingdom, Kingdom& defenderKingdom,
        XPSystemState& xpSystemState, std::uint32_t worldSeed,
        const GameConfig& config, EventLog& log, int turnNumber);
};
