#pragma once
#include <vector>
#include <SFML/System/Vector2.hpp>
#include "Kingdom/KingdomId.hpp"

struct GameSnapshot;
struct AITurnContext;

/// Marriage pursuit plan
struct MarriagePlan {
    bool pursuing = false;
    sf::Vector2i kingTarget{0, 0};
    sf::Vector2i bishopTarget{0, 0};
    sf::Vector2i pawnTarget{0, 0};
    int estimatedTurns = 999;
    bool canMarryNow = false; // all conditions met this turn
};

/// Special actions module — handles marriage pursuit
class AISpecialModule {
public:
    AISpecialModule() = default;

    /// Evaluate whether marriage is worth pursuing and build a plan.
    MarriagePlan evaluateMarriage(const GameSnapshot& s, KingdomId k) const;

    /// Find the best movement target for a piece pursuing marriage.
    /// Returns the cell this piece should move toward.
    sf::Vector2i getMarriageMoveTarget(const GameSnapshot& s, KingdomId k,
                                        int pieceId) const;

private:
    static std::vector<sf::Vector2i> getChurchCells(const GameSnapshot& s);
    static sf::Vector2i closestChurchCell(sf::Vector2i from,
                                           const std::vector<sf::Vector2i>& cells);
};
