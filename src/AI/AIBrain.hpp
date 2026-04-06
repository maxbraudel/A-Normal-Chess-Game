#pragma once
#include <string>

class Board;
class Kingdom;
class GameConfig;
class AIConfig;
struct AITurnContext;

enum class AIPhase {
    EARLY_GAME,   // Rush to resources, start production
    BUILD_UP,     // Accumulate army, secure resources
    MID_GAME,     // Expand, contest resources, skirmish
    AGGRESSION,   // March toward enemy king, checkmate
    ENDGAME,      // Few pieces, pure tactics
    CRISIS        // In check or king directly threatened
};

// Priority weights for each action category in a given phase
struct PhasePriorities {
    float economy   = 0.5f;
    float production = 0.5f;
    float building  = 0.5f;
    float attack    = 0.5f;
    float defense   = 0.5f;
};

class AIBrain {
public:
    AIBrain();

    // Evaluate game state and determine current phase (uses cached context)
    void update(const Board& board, const Kingdom& self, const Kingdom& enemy,
                const GameConfig& config, const AITurnContext& ctx, int turnNumber);

    AIPhase getPhase() const;
    const PhasePriorities& getPriorities() const;

    // Helpers for strategy modules
    float getMaterialScore(const Kingdom& kingdom) const;
    int countCombatPieces(const Kingdom& kingdom) const;
    bool hasIncome(const Kingdom& kingdom, const Board& board) const;
    bool hasSufficientMatingMaterial(const Kingdom& kingdom) const;

    std::string getPhaseName() const;

private:
    AIPhase m_phase;
    PhasePriorities m_priorities;
    int m_turnNumber;

    void determinePhase(const Board& board, const Kingdom& self, const Kingdom& enemy,
                        const GameConfig& config, const AITurnContext& ctx);
    void setPrioritiesForPhase();
};
