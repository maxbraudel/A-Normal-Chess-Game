#pragma once
#include "Units/PieceType.hpp"
#include <vector>

class Piece;
class Kingdom;
class Board;
class Building;
class GameConfig;

class XPSystem {
public:
    static void grantKillXP(Piece& killer, PieceType victim, const GameConfig& config);
    static void grantBlockDestroyXP(Piece& destroyer, const GameConfig& config);
    static void grantArenaXP(Kingdom& kingdom, const Board& board,
                              const std::vector<Building>& buildings, const GameConfig& config);
    static bool canUpgrade(const Piece& piece, PieceType target, const GameConfig& config);
    static void upgrade(Piece& piece, PieceType target);
};
