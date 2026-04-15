#pragma once
#include <vector>
#include <unordered_map>
#include <SFML/System/Vector2.hpp>
#include "AI/ThreatMap.hpp"

class Board;
class Kingdom;
class GameConfig;

// Pre-computed data cached once at the start of the AI turn.
// Passed to every strategy module to avoid redundant computations.
struct AITurnContext {
    ThreatMap enemyThreats;   // squares attacked by enemy pieces
    ThreatMap selfThreats;    // squares attacked by our pieces

    // Pre-computed move lists for all our pieces (keyed by piece id)
    std::unordered_map<int, std::vector<sf::Vector2i>> selfMoves;
    // Pre-computed move lists for all enemy pieces (keyed by piece id)
    std::unordered_map<int, std::vector<sf::Vector2i>> enemyMoves;

    // Cached resource cell positions (mines/farms not occupied by self)
    std::vector<sf::Vector2i> freeResourceCells;

    int mineIncomePerCell = 0;
    int farmIncomePerCell = 0;

    // Build the context — called once per AI turn
    void build(const Board& board, const Kingdom& self, const Kingdom& enemy,
               const GameConfig& config);
};
