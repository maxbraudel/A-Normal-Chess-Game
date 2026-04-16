#pragma once
#include <optional>
#include <vector>
#include "AI/GameSnapshot.hpp"
#include "AI/ThreatMap.hpp"

class Board;
class Kingdom;
class Building;
class GameConfig;

/// The ForwardModel lets the AI simulate the game state cheaply.
/// It creates snapshots from the real game, applies actions on snapshots,
/// and evaluates the resulting states — all without touching the real Board.
class ForwardModel {
public:
    // ---- Snapshot creation ----
    static GameSnapshot createSnapshot(const Board& board,
                                       const Kingdom& first,
                                       const Kingdom& second,
                                       const std::vector<Building>& publicBuildings,
                                       int turnNumber);

    // ---- Legal move generation on snapshots ----
    static std::vector<sf::Vector2i> getPseudoLegalMoves(const GameSnapshot& s,
                                                         const SnapPiece& piece,
                                                         int globalMaxRange);
    static std::vector<sf::Vector2i> getLegalMoves(const GameSnapshot& s,
                                                    const SnapPiece& piece,
                                                    int globalMaxRange);

    // ---- Atomic actions ----
    static bool applyMove(GameSnapshot& s, int pieceId, sf::Vector2i dest,
                          KingdomId mover, const GameConfig& config);
    static bool applyBuild(GameSnapshot& s, KingdomId k, BuildingType type,
                           sf::Vector2i pos, int sourceWidth, int sourceHeight,
                           int rotationQuarterTurns, int cost, int cellHP,
                           const GameConfig& config,
                           std::optional<int> buildId = std::nullopt);
    static bool applyProduce(GameSnapshot& s, int barracksId, PieceType type,
                             int cost, int productionTurns, KingdomId k);
    static bool applyAutomaticChurchCoronation(GameSnapshot& s, KingdomId k);
    static bool applyMarriage(GameSnapshot& s, KingdomId k);

    // ---- Turn advancement (income, production tick, spawns) ----
    static void advanceTurn(GameSnapshot& s, KingdomId k,
                            int mineIncomePerCell, int farmIncomePerCell,
                            int arenaXP, const GameConfig& config);

    // ---- Tactical queries ----
    static ThreatMap buildThreatMap(const GameSnapshot& s, KingdomId attacker,
                                    int globalMaxRange);
    static bool isInCheck(const GameSnapshot& s, KingdomId k, int globalMaxRange);
    static bool isCheckmate(const GameSnapshot& s, KingdomId k, int globalMaxRange);

private:
    // Movement helpers
    static std::vector<sf::Vector2i> getPawnMoves(const SnapPiece& piece,
                                                   const GameSnapshot& s);
    static std::vector<sf::Vector2i> getPawnThreatenedSquares(const SnapPiece& piece,
                                                               const GameSnapshot& s);
    static std::vector<sf::Vector2i> getKnightMoves(const SnapPiece& piece,
                                                     const GameSnapshot& s);
    static std::vector<sf::Vector2i> getDirectionalMoves(const SnapPiece& piece,
                                                          const GameSnapshot& s,
                                                          int dx, int dy, int maxRange);
    static std::vector<sf::Vector2i> getKingMoves(const SnapPiece& piece,
                                                   const GameSnapshot& s);

    static bool canLandOn(const GameSnapshot& s, sf::Vector2i pos, KingdomId mover);
    static sf::Vector2i findSpawnCell(const GameSnapshot& s,
                                      const SnapBuilding& barracks,
                                      const SnapKingdom& kingdom,
                                      PieceType type);
};
