#pragma once
#include <vector>

class Kingdom;
class Board;
class GameConfig;

class FormationSystem {
public:
    static bool canFormGroup(const Kingdom& kingdom, const std::vector<int>& pieceIds, const Board& board);
    static int formGroup(Kingdom& kingdom, const std::vector<int>& pieceIds);
    static void breakGroup(Kingdom& kingdom, int formationId);
    static std::vector<int> getFormationPieceIds(const Kingdom& kingdom, int formationId);
    static bool areAdjacent(const Board& board, const std::vector<int>& pieceIds, const Kingdom& kingdom);

private:
    static int s_nextFormationId;
};
