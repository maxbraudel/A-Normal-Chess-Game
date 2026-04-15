#pragma once
#include <optional>
#include <SFML/System/Vector2.hpp>
#include "AI/GameSnapshot.hpp"
#include "AI/TimeBudget.hpp"
#include "Kingdom/KingdomId.hpp"

struct AITurnContext;
class GameConfig;

/// A simple move action (pieceId + destination)
struct MateMove {
    int pieceId = -1;
    sf::Vector2i destination{0, 0};
};

/// Fast checkmate detection.
///  - findMateIn1(): optimized O(pieces*moves) with early exit
///  - findMateInN(): optional deeper search using alpha-beta on check-giving moves
class CheckmateSolver {
public:
    CheckmateSolver() = default;

    /// Find a move that delivers checkmate in 1. Returns nullopt if none found.
    std::optional<MateMove> findMateIn1(const GameSnapshot& snapshot,
                                         KingdomId aiKingdom,
                                         int globalMaxRange,
                                         const GameConfig& config) const;

    /// Deeper mate search (2-3 moves). Time-budgeted.
    std::optional<MateMove> findMateInN(const GameSnapshot& snapshot,
                                         KingdomId aiKingdom,
                                         int maxDepth,
                                         int globalMaxRange,
                                         int budgetMs,
                                         const GameConfig& config) const;

private:
    /// Would moving piece to dest give check to the enemy king?
    static bool wouldGiveCheck(const GameSnapshot& s, const SnapPiece& piece,
                               sf::Vector2i dest, sf::Vector2i eKingPos,
                               int globalMaxRange);

    /// Compute escape squares for the enemy king
    static std::vector<sf::Vector2i> computeEscapeSquares(const GameSnapshot& s,
                                                           sf::Vector2i eKingPos,
                                                           KingdomId enemyKingdom);

    /// Can an enemy piece block or capture to defend the check?
    static bool canDefendCheck(const GameSnapshot& s, KingdomId defender,
                               int globalMaxRange,
                               const GameConfig& config);

    /// Alpha-beta mate search (on check-giving moves only)
    std::optional<MateMove> alphaBetaMate(const GameSnapshot& s, KingdomId ai,
                                           int depth, int maxDepth,
                                           int globalMaxRange,
                                           TimeBudget& timer,
                                           const GameConfig& config) const;
};
