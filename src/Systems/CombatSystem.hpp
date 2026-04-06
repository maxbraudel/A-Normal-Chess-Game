#pragma once
#include <SFML/System/Vector2.hpp>

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
    };

    static CombatResult resolve(
        Piece& attacker, Board& board, sf::Vector2i target,
        Kingdom& attackerKingdom, Kingdom& defenderKingdom,
        const GameConfig& config, EventLog& log, int turnNumber);
};
