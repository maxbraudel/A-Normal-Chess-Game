#pragma once

class Kingdom;
class Board;
class Building;
class EventLog;

class MarriageSystem {
public:
    static bool canMarry(const Kingdom& kingdom, const Board& board, const Building& church);
    static void performMarriage(Kingdom& kingdom, const Board& board, const Building& church,
                                 EventLog& log, int turnNumber);
};
