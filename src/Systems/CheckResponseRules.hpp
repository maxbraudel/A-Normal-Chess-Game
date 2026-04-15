#pragma once

#include <string>
#include <vector>

#include <SFML/System/Vector2.hpp>

#include "Systems/TurnCommand.hpp"

class Board;
class GameConfig;
class Kingdom;
class Piece;

struct CheckTurnValidation {
    bool valid = true;
    bool activeKingInCheck = false;
    bool hasAnyLegalResponse = false;
    bool hasQueuedMove = false;
    std::string errorMessage;
};

class CheckResponseRules {
public:
    static bool isActiveKingInCheck(Kingdom& kingdom,
                                    Board& board,
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

    static CheckTurnValidation validatePendingTurn(Kingdom& activeKingdom,
                                                   Board& board,
                                                   const std::vector<TurnCommand>& pendingCommands,
                                                   const GameConfig& config);
};