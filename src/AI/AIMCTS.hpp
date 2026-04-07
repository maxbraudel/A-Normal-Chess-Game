#pragma once
#include <vector>
#include <memory>
#include <SFML/System/Vector2.hpp>
#include "AI/GameSnapshot.hpp"
#include "AI/AIStrategy.hpp"
#include "AI/TimeBudget.hpp"
#include "Units/PieceType.hpp"
#include "Buildings/BuildingType.hpp"
#include "Kingdom/KingdomId.hpp"

struct EvalWeights;

/// Action that can be taken in a turn
struct MCTSAction {
    enum Type { MOVE, BUILD, PRODUCE, MARRY, END_TURN } type = END_TURN;
    int pieceId = -1;              // for MOVE
    sf::Vector2i destination{0,0}; // for MOVE / BUILD
    BuildingType bldType = BuildingType::Barracks; // for BUILD
    PieceType prodType = PieceType::Pawn;          // for PRODUCE
    int barracksId = -1;           // for PRODUCE
};

/// MCTS node
struct MCTSNode {
    GameSnapshot state;
    MCTSNode* parent = nullptr;
    std::vector<std::unique_ptr<MCTSNode>> children;
    MCTSAction action;

    int visits = 0;
    float totalScore = 0.0f;

    float averageScore() const { return visits > 0 ? totalScore / static_cast<float>(visits) : 0.0f; }
};

/// Monte Carlo Tree Search for tactical move selection
class AIMCTS {
public:
    AIMCTS() = default;

    /// Run MCTS and return the best action found within budgetMs.
    MCTSAction search(const GameSnapshot& rootState,
                      KingdomId aiKingdom,
                      StrategicObjective objective,
                      int globalMaxRange,
                      const EvalWeights& weights,
                      int budgetMs);

private:
    // MCTS phases
    MCTSNode* selection(MCTSNode* node);
    MCTSNode* expansion(MCTSNode* node, KingdomId aiKingdom,
                        StrategicObjective objective, int globalMaxRange);
    float simulation(const GameSnapshot& state, KingdomId aiKingdom,
                     int globalMaxRange, const EvalWeights& weights, int rolloutDepth);
    void backpropagation(MCTSNode* node, float score);

    // Action generation with pruning
    std::vector<MCTSAction> generateCandidateActions(
        const GameSnapshot& s, KingdomId k,
        StrategicObjective objective, int globalMaxRange);

    // Move relevance scoring for pruning
    float scoreMoveRelevance(const GameSnapshot& s, const SnapPiece& piece,
                              sf::Vector2i dest, StrategicObjective obj);

    // Rollout helpers
    MCTSAction selectRolloutAction(const GameSnapshot& s, KingdomId active,
                                    int globalMaxRange);
    void applyAction(GameSnapshot& s, const MCTSAction& action, KingdomId k);

    // UCB1
    static float ucb1(const MCTSNode& child, int parentVisits, float C = 1.41f);

    // Constants
    static constexpr float RELEVANCE_THRESHOLD = 5.0f;
    static constexpr int MAX_CHILDREN_PER_NODE = 30;
    static constexpr int MAX_ITERATIONS = 500;
    static constexpr int DEFAULT_ROLLOUT_DEPTH = 6;
};
