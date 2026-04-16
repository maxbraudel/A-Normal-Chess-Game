#pragma once

#include "Kingdom/KingdomId.hpp"

class Kingdom;
class Board;
class Building;
class EventLog;
struct GameSnapshot;

class MarriageSystem {
public:
    static bool canPerformChurchCoronation(const Kingdom& kingdom, const Board& board,
                                           const Building& church);
    static bool performChurchCoronation(Kingdom& kingdom, const Board& board,
                                        const Building& church, EventLog& log,
                                        int turnNumber);

    static bool canPerformChurchCoronation(const GameSnapshot& snapshot, KingdomId kingdomId);
    static bool applyChurchCoronation(GameSnapshot& snapshot, KingdomId kingdomId);

    static bool canMarry(const Kingdom& kingdom, const Board& board, const Building& church);
    static void performMarriage(Kingdom& kingdom, const Board& board, const Building& church,
                                 EventLog& log, int turnNumber);
};
