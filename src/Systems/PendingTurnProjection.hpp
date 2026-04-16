#pragma once

#include <string>
#include <vector>

#include "AI/GameSnapshot.hpp"
#include "Systems/TurnCommand.hpp"

class Board;
class Building;
class GameConfig;
class Kingdom;

struct PendingTurnProjectionResult {
    GameSnapshot snapshot;
    bool valid = true;
    std::string errorMessage;
};

enum class PendingTurnInvalidCommandPolicy {
    FailFast,
    DropInvalidBuilds
};

struct PendingTurnDroppedCommand {
    std::size_t originalIndex = 0;
    TurnCommand command;
    std::string errorMessage;
};

struct PendingTurnNormalizationResult {
    GameSnapshot snapshot;
    std::vector<TurnCommand> normalizedCommands;
    std::vector<PendingTurnDroppedCommand> droppedCommands;
    bool valid = true;
    std::string errorMessage;
};

class PendingTurnProjection {
public:
    static void initializeBudgets(GameSnapshot& snapshot,
                                  KingdomId activeKingdom,
                                  const GameConfig& config);

    static PendingTurnProjectionResult project(const Board& board,
                                               const Kingdom& activeKingdom,
                                               const Kingdom& enemyKingdom,
                                               const std::vector<Building>& publicBuildings,
                                               int turnNumber,
                                               const std::vector<TurnCommand>& commands,
                                               const GameConfig& config);

    static PendingTurnNormalizationResult normalize(const Board& board,
                                                    const Kingdom& activeKingdom,
                                                    const Kingdom& enemyKingdom,
                                                    const std::vector<Building>& publicBuildings,
                                                    int turnNumber,
                                                    const std::vector<TurnCommand>& commands,
                                                    const GameConfig& config,
                                                    PendingTurnInvalidCommandPolicy invalidCommandPolicy);

    static PendingTurnProjectionResult projectWithCandidate(const Board& board,
                                                            const Kingdom& activeKingdom,
                                                            const Kingdom& enemyKingdom,
                                                            const std::vector<Building>& publicBuildings,
                                                            int turnNumber,
                                                            const std::vector<TurnCommand>& commands,
                                                            const TurnCommand& candidate,
                                                            const GameConfig& config);

    static bool canAppendCommand(const Board& board,
                                 const Kingdom& activeKingdom,
                                 const Kingdom& enemyKingdom,
                                 const std::vector<Building>& publicBuildings,
                                 int turnNumber,
                                 const std::vector<TurnCommand>& commands,
                                 const TurnCommand& candidate,
                                 const GameConfig& config,
                                 std::string* errorMessage = nullptr);

    static std::vector<sf::Vector2i> projectedPseudoLegalMovesForPiece(
        const Board& board,
        const Kingdom& activeKingdom,
        const Kingdom& enemyKingdom,
        const std::vector<Building>& publicBuildings,
        int turnNumber,
        const std::vector<TurnCommand>& commands,
        int pieceId,
        const GameConfig& config);
};