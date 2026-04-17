#pragma once
#include <set>
#include <SFML/System/Vector2.hpp>
#include "Kingdom/KingdomId.hpp"
#include "Projection/ThreatMap.hpp"

class Board;
class Kingdom;
class GameConfig;

struct Vec2iCompare {
    bool operator()(const sf::Vector2i& a, const sf::Vector2i& b) const {
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y;
    }
};

class CheckSystem {
public:
    // ---- Legacy signatures (used by Game.cpp — find kingdoms by scanning board) ----
    static bool isInCheck(KingdomId kingdomId, const Board& board, const GameConfig& config);
    static bool isCheckmate(KingdomId kingdomId, Board& board, const GameConfig& config);
    static bool isSafeSquare(sf::Vector2i pos, KingdomId kingdomId, const Board& board, const GameConfig& config);
    static std::set<sf::Vector2i, Vec2iCompare> getThreatenedSquares(
        KingdomId attackerKingdom, const Board& board, const GameConfig& config);

    // ---- Fast overloads (use Kingdom refs — no board scan to find pieces) ----
    static bool isInCheckFast(const Kingdom& kingdom, const Kingdom& enemy,
                              const Board& board, const GameConfig& config);
    static ThreatMap buildThreatMap(const Kingdom& attacker,
                                    const Board& board, const GameConfig& config);
};
