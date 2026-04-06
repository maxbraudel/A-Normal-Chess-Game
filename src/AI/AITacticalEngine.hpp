#pragma once
#include <vector>
#include <SFML/System/Vector2.hpp>
#include "Units/PieceType.hpp"
#include "Kingdom/KingdomId.hpp"

class Board;
class Kingdom;
class GameConfig;
class Piece;
struct AITurnContext;

// A scored move for the tactical engine
struct ScoredMove {
    int pieceId = -1;
    sf::Vector2i from{0, 0};
    sf::Vector2i to{0, 0};
    float score = 0.0f;
};

// Heuristic-based move evaluator — no tree search, uses cached context.
// Industry-standard approach used in Civilization, Fire Emblem, Total War, etc.
class AITacticalEngine {
public:
    AITacticalEngine();

    // Find the best move using heuristic scoring (O(pieces * moves), no minimax)
    ScoredMove findBestMove(const Board& board, const Kingdom& self, const Kingdom& enemy,
                            const GameConfig& config, const AITurnContext& ctx);

    // Quick check: is destination safe? O(1) lookup into cached ThreatMap
    bool isMoveSafe(sf::Vector2i destination, const AITurnContext& ctx) const;

    // Check for checkmate-in-1 only (O(pieces * moves), no deep search)
    ScoredMove findCheckmateIn1(Board& board, Kingdom& self, Kingdom& enemy,
                                const GameConfig& config, const AITurnContext& ctx);

    // Static evaluation helpers
    static float pieceValue(PieceType type);

private:
    // Score a single candidate move using heuristics
    float scoreMove(const Piece& piece, sf::Vector2i dest,
                    const Board& board, const Kingdom& self, const Kingdom& enemy,
                    const GameConfig& config, const AITurnContext& ctx) const;
};
