#pragma once

class Board;
class Kingdom;
class GameConfig;
class AIConfig;

class AIEvaluator {
public:
    static float evaluate(const Board& board, const Kingdom& self, const Kingdom& enemy,
                           const GameConfig& config);

private:
    static float scoreMaterial(const Kingdom& kingdom);
    static float scoreIncome(const Kingdom& kingdom, const Board& board, const GameConfig& config);
    static float scoreKingSafety(const Kingdom& kingdom, const Board& board, const GameConfig& config);
    static float scoreTerritorialControl(const Kingdom& kingdom, const Board& board);
    static float scoreBuildings(const Kingdom& kingdom);
    static float scoreThreats(const Kingdom& kingdom, const Kingdom& enemy,
                               const Board& board, const GameConfig& config);
};
