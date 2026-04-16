#pragma once

#include <string>
#include <vector>

#include <SFML/System/Vector2.hpp>

#include "Systems/TurnCommand.hpp"
#include "Systems/TurnValidationContext.hpp"

class Board;
class Building;
class GameConfig;
class Kingdom;
class Piece;

struct CheckTurnValidation {
    bool valid = true;
    bool activeKingInCheck = false;
    bool projectedKingInCheck = false;
    bool hasAnyLegalResponse = false;
    bool hasQueuedMove = false;
    bool bankrupt = false;
    int projectedEndingGold = 0;
    std::string errorMessage;
};

class CheckResponseRules {
public:
    static bool isActiveKingInCheck(const TurnValidationContext& context,
                                    const std::vector<TurnCommand>& pendingCommands);

    static bool isActiveKingInCheck(const Kingdom& activeKingdom,
                                    const Kingdom& enemyKingdom,
                                    const Board& board,
                                    const std::vector<Building>& publicBuildings,
                                    int turnNumber,
                                    const std::vector<TurnCommand>& pendingCommands,
                                    const GameConfig& config);

    static std::vector<sf::Vector2i> filterLegalMovesForPiece(Piece& piece,
                                                              Board& board,
                                                              const GameConfig& config);

    static bool moveKeepsKingSafe(Piece& piece,
                                  sf::Vector2i origin,
                                  sf::Vector2i destination,
                                  Board& board,
                                  const GameConfig& config);

    static bool hasAnyLegalResponse(Kingdom& kingdom,
                                    Board& board,
                                    const GameConfig& config);

    static CheckTurnValidation validatePendingTurn(const TurnValidationContext& context,
                                                   const std::vector<TurnCommand>& pendingCommands);

    static CheckTurnValidation validatePendingTurn(const Kingdom& activeKingdom,
                                                   const Kingdom& enemyKingdom,
                                                   const Board& board,
                                                   const std::vector<Building>& publicBuildings,
                                                   int turnNumber,
                                                   const std::vector<TurnCommand>& pendingCommands,
                                                   const GameConfig& config);
};