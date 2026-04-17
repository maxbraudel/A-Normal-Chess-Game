#include "AI/ForwardModel.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Buildings/Building.hpp"
#include "Units/Piece.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/BuildReachRules.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/MarriageSystem.hpp"
#include "Systems/ProductionSpawnRules.hpp"
#include "Systems/StructureIntegrityRules.hpp"
#include "Systems/TurnPointRules.hpp"
#include "Systems/XPSystem.hpp"
#include <cmath>
#include <algorithm>

namespace {

SnapTurnBudget toSnapTurnBudget(const TurnPointBudget& budget) {
    SnapTurnBudget snapBudget;
    snapBudget.movementPointsMax = budget.movementPointsMax;
    snapBudget.movementPointsRemaining = budget.movementPointsRemaining;
    snapBudget.buildPointsMax = budget.buildPointsMax;
    snapBudget.buildPointsRemaining = budget.buildPointsRemaining;
    return snapBudget;
}

bool isBlockingWallCell(const SnapBuilding* building, sf::Vector2i pos) {
    if (!building) {
        return false;
    }

    const int localX = pos.x - building->origin.x;
    const int localY = pos.y - building->origin.y;
    return StructureIntegrityRules::isWallCellBlocking(*building, localX, localY);
}

bool isEnemyCapturableBuildingCell(const GameSnapshot& snapshot,
                                   sf::Vector2i pos,
                                   KingdomId mover) {
    const SnapBuilding* building = snapshot.buildingAt(pos);
    return building && !building->isNeutral && building->owner != mover;
}

void processEnemyStructureOccupancy(GameSnapshot& snapshot,
                                    KingdomId activeKingdom,
                                    const GameConfig& config) {
    SnapKingdom& active = snapshot.kingdom(activeKingdom);
    for (auto& piece : active.pieces) {
        SnapBuilding* building = snapshot.buildingAt(piece.position);
        if (!building || building->isNeutral || building->owner == activeKingdom) {
            continue;
        }

        const int localX = piece.position.x - building->origin.x;
        const int localY = piece.position.y - building->origin.y;
        const StructureOccupancyResult result = StructureIntegrityRules::applyEnemyOccupancy(
            *building, localX, localY, config);
        if (result != StructureOccupancyResult::None) {
            piece.xp += XPSystem::sampleBlockDestroyXP(snapshot.xpSystemState, snapshot.worldSeed, config);
        }
    }
}

void processFriendlyRepairs(GameSnapshot& snapshot,
                            KingdomId activeKingdom,
                            const GameConfig& config) {
    SnapKingdom& active = snapshot.kingdom(activeKingdom);
    for (auto& building : active.buildings) {
        if (!StructureIntegrityRules::isRepairableOwnedStructureType(building.type)) {
            continue;
        }

        const int footprintWidth = building.getFootprintWidth();
        const int footprintHeight = building.getFootprintHeight();
        for (int localY = 0; localY < footprintHeight; ++localY) {
            for (int localX = 0; localX < footprintWidth; ++localX) {
                if (!building.isCellDestroyed(localX, localY)) {
                    continue;
                }

                const sf::Vector2i cellPos{building.origin.x + localX, building.origin.y + localY};
                const SnapPiece* occupant = active.getPieceAt(cellPos);
                if (!occupant || !isBuildSupportPieceType(occupant->type)) {
                    continue;
                }

                const int repairCost = StructureIntegrityRules::repairCostPerCell(building.type, config);
                if (active.gold < repairCost) {
                    continue;
                }

                if (StructureIntegrityRules::repairDestroyedCell(building, localX, localY, config)) {
                    active.gold -= repairCost;
                }
            }
        }
    }
}

void removeDestroyedStructures(SnapKingdom& kingdom) {
    kingdom.buildings.erase(
        std::remove_if(kingdom.buildings.begin(), kingdom.buildings.end(),
                       [](const SnapBuilding& building) {
                           return building.isDestroyed()
                               && StructureIntegrityRules::shouldRemoveWhenFullyDestroyed(building.type);
                       }),
        kingdom.buildings.end());
}

void completeUnderConstructionBuildings(SnapKingdom& kingdom) {
    for (auto& building : kingdom.buildings) {
        if (building.isUnderConstruction()) {
            building.state = BuildingState::Completed;
        }
    }
}

} // namespace

// ========================================================================
// Snapshot creation from real game state
// ========================================================================

static SnapPiece toSnap(const Piece& p) {
    return {p.id, p.type, p.kingdom, p.position, p.xp};
}

static SnapBuilding toSnapBuilding(const Building& b) {
    SnapBuilding sb;
    sb.id = b.id;
    sb.type = b.type;
    sb.owner = b.owner;
    sb.isNeutral = b.isNeutral;
    sb.origin = b.origin;
    sb.width = b.width;
    sb.height = b.height;
    sb.rotationQuarterTurns = b.rotationQuarterTurns;
    sb.flipMask = b.flipMask;
    sb.state = b.state;
    sb.cellHP = b.cellHP;
    sb.cellBreachState = b.cellBreachState;
    sb.isProducing = b.isProducing;
    sb.producingType = static_cast<PieceType>(b.producingType);
    sb.turnsRemaining = b.turnsRemaining;
    return sb;
}

GameSnapshot ForwardModel::createSnapshot(const Board& board,
                                          const Kingdom& first,
                                          const Kingdom& second,
                                          const std::vector<Building>& publicBuildings,
                                          int turnNumber,
                                          std::uint32_t worldSeed,
                                          XPSystemState xpSystemState) {
    GameSnapshot s;

    // Build shared terrain grid
    auto tg = std::make_shared<TerrainGrid>();
    tg->radius = board.getRadius();
    tg->diameter = board.getDiameter();
    tg->types.resize(tg->diameter, std::vector<CellType>(tg->diameter, CellType::Void));
    tg->inCircle.resize(tg->diameter, std::vector<bool>(tg->diameter, false));
    for (int y = 0; y < tg->diameter; ++y) {
        for (int x = 0; x < tg->diameter; ++x) {
            const Cell& c = board.getCell(x, y);
            tg->types[y][x] = c.type;
            tg->inCircle[y][x] = c.isInCircle;
        }
    }
    s.terrain = tg;

    // Kingdoms are assigned by their real ids, not by call-site order.
    s.white.id = KingdomId::White;
    s.black.id = KingdomId::Black;

    auto assignKingdom = [&](const Kingdom& kingdom) {
        SnapKingdom& target = (kingdom.id == KingdomId::White) ? s.white : s.black;
        target.gold = kingdom.gold;
        target.movementPointsMaxBonus = kingdom.movementPointsMaxBonus;
        target.buildPointsMaxBonus = kingdom.buildPointsMaxBonus;
        target.hasSpawnedBishop = kingdom.hasSpawnedBishop;
        target.lastBishopSpawnParity = kingdom.lastBishopSpawnParity;
        target.pieces.clear();
        target.buildings.clear();
        for (const auto& p : kingdom.pieces) target.pieces.push_back(toSnap(p));
        for (const auto& b : kingdom.buildings) target.buildings.push_back(toSnapBuilding(b));
    };

    assignKingdom(first);
    assignKingdom(second);

    // Public buildings
    for (auto& b : publicBuildings) s.publicBuildings.push_back(toSnapBuilding(b));

    s.turnNumber = turnNumber;
    s.worldSeed = worldSeed;
    s.xpSystemState = xpSystemState;
    return s;
}

// ========================================================================
// Movement helpers
// ========================================================================

bool ForwardModel::canLandOn(const GameSnapshot& s, sf::Vector2i pos, KingdomId mover) {
    if (!s.isTraversable(pos.x, pos.y)) return false;
    const SnapBuilding* building = s.buildingAt(pos);
    if (isBlockingWallCell(building, pos)) {
        return !building->isNeutral && building->owner != mover;
    }
    const SnapPiece* occ = s.pieceAt(pos);
    if (occ && occ->kingdom == mover) return false; // can't land on own piece
    return true;
}

std::vector<sf::Vector2i> ForwardModel::getPawnMoves(const SnapPiece& piece,
                                                      const GameSnapshot& s) {
    std::vector<sf::Vector2i> moves;
    static const int orthogonalDirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    static const int diagonalDirs[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};

    for (const auto& direction : orthogonalDirs) {
        const sf::Vector2i dest{piece.position.x + direction[0], piece.position.y + direction[1]};
        if (!s.isTraversable(dest.x, dest.y)) {
            continue;
        }

        if (s.pieceAt(dest)) {
            continue;
        }
        if (isEnemyCapturableBuildingCell(s, dest, piece.kingdom)) {
            continue;
        }
        if (isBlockingWallCell(s.buildingAt(dest), dest)) {
            continue;
        }

        moves.push_back(dest);
    }

    for (const auto& direction : diagonalDirs) {
        const sf::Vector2i dest{piece.position.x + direction[0], piece.position.y + direction[1]};
        if (!s.isTraversable(dest.x, dest.y)) {
            continue;
        }

        const SnapPiece* occupant = s.pieceAt(dest);
        if (occupant && occupant->kingdom != piece.kingdom) {
            moves.push_back(dest);
            continue;
        }
        if (isEnemyCapturableBuildingCell(s, dest, piece.kingdom)) {
            moves.push_back(dest);
        }
    }

    return moves;
}

std::vector<sf::Vector2i> ForwardModel::getPawnThreatenedSquares(const SnapPiece& piece,
                                                                  const GameSnapshot& s) {
    std::vector<sf::Vector2i> moves;
    static const int diagonalDirs[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    for (const auto& direction : diagonalDirs) {
        const sf::Vector2i dest{piece.position.x + direction[0], piece.position.y + direction[1]};
        if (s.isTraversable(dest.x, dest.y)) {
            moves.push_back(dest);
        }
    }
    return moves;
}

std::vector<sf::Vector2i> ForwardModel::getKnightMoves(const SnapPiece& piece,
                                                         const GameSnapshot& s) {
    std::vector<sf::Vector2i> moves;
    static const int offsets[8][2] = {
        {-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}
    };
    for (auto& o : offsets) {
        sf::Vector2i dest{piece.position.x + o[0], piece.position.y + o[1]};
        if (canLandOn(s, dest, piece.kingdom))
            moves.push_back(dest);
    }
    return moves;
}

std::vector<sf::Vector2i> ForwardModel::getDirectionalMoves(const SnapPiece& piece,
                                                              const GameSnapshot& s,
                                                              int dx, int dy, int maxRange) {
    std::vector<sf::Vector2i> moves;
    for (int i = 1; i <= maxRange; ++i) {
        sf::Vector2i dest{piece.position.x + dx * i, piece.position.y + dy * i};
        if (!s.isTraversable(dest.x, dest.y)) break;

        const SnapBuilding* building = s.buildingAt(dest);
        if (isBlockingWallCell(building, dest)) {
            if (!building->isNeutral && building->owner != piece.kingdom) {
                moves.push_back(dest);
            }
            break;
        }

        const SnapPiece* occ = s.pieceAt(dest);
        if (occ) {
            if (occ->kingdom != piece.kingdom)
                moves.push_back(dest); // capture
            break; // blocked
        }
        // Check for wall/building blocking (non-piece occupant)
        // Buildings don't physically block movement on a cell basis for sliding pieces
        // — only pieces do. Wall cells are traversable unless a piece is there.
        moves.push_back(dest);
    }
    return moves;
}

std::vector<sf::Vector2i> ForwardModel::getKingMoves(const SnapPiece& piece,
                                                       const GameSnapshot& s) {
    std::vector<sf::Vector2i> moves;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            sf::Vector2i dest{piece.position.x + dx, piece.position.y + dy};
            if (canLandOn(s, dest, piece.kingdom)) {
                // Don't let king move adjacent to enemy king
                const SnapPiece* enemyKing = s.enemyKingdom(piece.kingdom).getKing();
                if (enemyKing) {
                    int ekdx = std::abs(dest.x - enemyKing->position.x);
                    int ekdy = std::abs(dest.y - enemyKing->position.y);
                    if (ekdx <= 1 && ekdy <= 1) continue; // too close to enemy king
                }
                moves.push_back(dest);
            }
        }
    }
    return moves;
}

std::vector<sf::Vector2i> ForwardModel::getPseudoLegalMoves(const GameSnapshot& s,
                                                            const SnapPiece& piece,
                                                            int globalMaxRange) {
    std::vector<sf::Vector2i> moves;

    switch (piece.type) {
        case PieceType::Pawn:
            moves = getPawnMoves(piece, s);
            break;
        case PieceType::Knight:
            moves = getKnightMoves(piece, s);
            break;
        case PieceType::Bishop:
            for (auto& d : std::vector<std::pair<int,int>>{{-1,-1},{-1,1},{1,-1},{1,1}}) {
                auto dm = getDirectionalMoves(piece, s, d.first, d.second, globalMaxRange);
                moves.insert(moves.end(), dm.begin(), dm.end());
            }
            break;
        case PieceType::Rook:
            for (auto& d : std::vector<std::pair<int,int>>{{0,-1},{0,1},{-1,0},{1,0}}) {
                auto dm = getDirectionalMoves(piece, s, d.first, d.second, globalMaxRange);
                moves.insert(moves.end(), dm.begin(), dm.end());
            }
            break;
        case PieceType::Queen:
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    auto dm = getDirectionalMoves(piece, s, dx, dy, globalMaxRange);
                    moves.insert(moves.end(), dm.begin(), dm.end());
                }
            }
            break;
        case PieceType::King:
            moves = getKingMoves(piece, s);
            break;
    }

    const SnapPiece* enemyKing = s.enemyKingdom(piece.kingdom).getKing();
    if (enemyKing) {
        moves.erase(
            std::remove(moves.begin(), moves.end(), enemyKing->position),
            moves.end());
    }

    return moves;
}

std::vector<sf::Vector2i> ForwardModel::getLegalMoves(const GameSnapshot& s,
                                                       const SnapPiece& piece,
                                                       int globalMaxRange) {
    std::vector<sf::Vector2i> legalMoves;
    const std::vector<sf::Vector2i> pseudoLegalMoves = getPseudoLegalMoves(s, piece, globalMaxRange);
    for (const sf::Vector2i& destination : pseudoLegalMoves) {
        GameSnapshot sim = s.clone();
        SnapPiece* simPiece = sim.kingdom(piece.kingdom).getPieceById(piece.id);
        if (!simPiece) {
            continue;
        }

        SnapKingdom& simEnemy = sim.enemyKingdom(piece.kingdom);
        SnapPiece* victim = simEnemy.getPieceAt(destination);
        if (victim) {
            if (victim->type == PieceType::King) {
                continue;
            }
            simEnemy.removePiece(victim->id);
        }

        simPiece->position = destination;
        if (!isInCheck(sim, piece.kingdom, globalMaxRange)) {
            legalMoves.push_back(destination);
        }
    }

    return legalMoves;
}

// ========================================================================
// Atomic actions
// ========================================================================

bool ForwardModel::applyMove(GameSnapshot& s, int pieceId, sf::Vector2i dest,
                             KingdomId mover, const GameConfig& config) {
    SnapKingdom& myK = s.kingdom(mover);
    SnapPiece* piece = myK.getPieceById(pieceId);
    if (!piece) return false;

    SnapTurnBudget& budget = s.turnBudget(mover);
    if (budget.movementPointsMax <= 0 && budget.buildPointsMax <= 0
        && budget.pieceMoveCounts.empty()) {
        budget = toSnapTurnBudget(TurnPointRules::makeBudget(
            config,
            myK.movementPointsMaxBonus,
            myK.buildPointsMaxBonus));
    }
    const int moveAllowance = TurnPointRules::moveAllowance(piece->type, config);
    if (budget.moveCountForPiece(pieceId) >= moveAllowance) return false;

    const int moveCost = TurnPointRules::movementCost(piece->type, config);
    if (budget.movementPointsRemaining < moveCost) return false;

    // Capture enemy piece at destination
    SnapKingdom& enemyK = s.enemyKingdom(mover);
    SnapPiece* victim = enemyK.getPieceAt(dest);
    if (victim) {
        if (victim->type == PieceType::King) return false; // illegal
        piece->xp += XPSystem::sampleKillXP(victim->type, s.xpSystemState, s.worldSeed, config);
        enemyK.removePiece(victim->id);
    }

    piece->position = dest;
    budget.movementPointsRemaining -= moveCost;
    budget.setMoveCountForPiece(pieceId, budget.moveCountForPiece(pieceId) + 1);
    return true;
}

bool ForwardModel::applyBuild(GameSnapshot& s, KingdomId k, BuildingType type,
                              sf::Vector2i pos, int sourceWidth, int sourceHeight,
                              int rotationQuarterTurns, int cost, int cellHPValue,
                              const GameConfig& config,
                              std::optional<int> buildId) {
    SnapKingdom& myK = s.kingdom(k);
    SnapTurnBudget& budget = s.turnBudget(k);
    if (budget.movementPointsMax <= 0 && budget.buildPointsMax <= 0
        && budget.pieceMoveCounts.empty()) {
        budget = toSnapTurnBudget(TurnPointRules::makeBudget(
            config,
            myK.movementPointsMaxBonus,
            myK.buildPointsMaxBonus));
    }
    const int buildPointCost = TurnPointRules::buildCost(type, config);
    if (budget.buildPointsRemaining < buildPointCost) return false;
    if (myK.gold < cost) return false;

    const int footprintWidth = Building::getFootprintWidthFor(
        sourceWidth, sourceHeight, rotationQuarterTurns);
    const int footprintHeight = Building::getFootprintHeightFor(
        sourceWidth, sourceHeight, rotationQuarterTurns);

    // Validate space
    for (int dy = 0; dy < footprintHeight; ++dy) {
        for (int dx = 0; dx < footprintWidth; ++dx) {
            int cx = pos.x + dx, cy = pos.y + dy;
            if (!s.isTraversable(cx, cy)) return false;
            if (s.pieceAt({cx, cy})) return false;
            if (s.buildingAt({cx, cy})) return false;
        }
    }

    if (!footprintHasAdjacentBuilder(pos, footprintWidth, footprintHeight,
                                     collectBuilderPositions(myK.pieces))) {
        return false;
    }

    myK.gold -= cost;
    budget.buildPointsRemaining -= buildPointCost;

    SnapBuilding bld;
    static int nextSnapBuildId = 90000;
    bld.id = buildId.has_value() ? *buildId : nextSnapBuildId++;
    bld.type = type;
    bld.owner = k;
    bld.isNeutral = false;
    bld.origin = pos;
    bld.width = sourceWidth;
    bld.height = sourceHeight;
    bld.rotationQuarterTurns = rotationQuarterTurns;
    bld.flipMask = 0;
    bld.state = BuildingState::UnderConstruction;
    bld.cellHP.assign(sourceWidth * sourceHeight, cellHPValue);
    bld.cellBreachState.assign(sourceWidth * sourceHeight, 0);
    myK.buildings.push_back(bld);
    return true;
}

bool ForwardModel::applyProduce(GameSnapshot& s, int barracksId, PieceType type,
                                 int cost, int productionTurns, KingdomId k) {
    SnapKingdom& myK = s.kingdom(k);
    SnapBuilding* barracks = myK.getBuildingById(barracksId);
    if (!barracks || !barracks->isUsable() || barracks->isProducing) return false;
    if (myK.gold < cost) return false;

    myK.gold -= cost;
    barracks->isProducing = true;
    barracks->producingType = type;
    barracks->turnsRemaining = productionTurns;
    return true;
}

bool ForwardModel::applyAutomaticChurchCoronation(GameSnapshot& s, KingdomId k) {
    return MarriageSystem::applyChurchCoronation(s, k);
}

bool ForwardModel::applyMarriage(GameSnapshot& s, KingdomId k) {
    return applyAutomaticChurchCoronation(s, k);
}

// ========================================================================
// Turn advancement
// ========================================================================

sf::Vector2i ForwardModel::findSpawnCell(const GameSnapshot& s,
                                         const SnapBuilding& barracks,
                                         const SnapKingdom& kingdom,
                                         PieceType type) {
    const std::optional<int> preferredParity = (type == PieceType::Bishop)
        ? kingdom.preferredNextBishopSpawnParity()
        : std::nullopt;

    return ProductionSpawnRules::findSpawnCell(
        barracks.origin,
        barracks.getFootprintWidth(),
        barracks.getFootprintHeight(),
        s.getDiameter(),
        [&s](const sf::Vector2i& pos) {
            if (!s.isTraversable(pos.x, pos.y)) return false;
            if (s.pieceAt(pos)) return false;
            return s.buildingAt(pos) == nullptr;
        },
        preferredParity);
}

void ForwardModel::advanceTurn(GameSnapshot& s, KingdomId k,
                               int mineIncomePerCell, int farmIncomePerCell,
                               const GameConfig& config) {
    (void)mineIncomePerCell;
    (void)farmIncomePerCell;

    SnapKingdom& myK = s.kingdom(k);

    processEnemyStructureOccupancy(s, k, config);
    applyAutomaticChurchCoronation(s, k);

    // 1. Advance production timers and spawn
    for (auto& b : myK.buildings) {
        if (!b.isProducing || b.isDestroyed()) continue;
        b.turnsRemaining--;
        if (b.turnsRemaining <= 0) {
            const PieceType producedType = b.producingType;
            sf::Vector2i spawnPos = findSpawnCell(s, b, myK, producedType);
            if (spawnPos.x >= 0) {
                static int nextSnapPieceId = 80000;
                SnapPiece np;
                np.id = nextSnapPieceId++;
                np.type = producedType;
                np.kingdom = k;
                np.position = spawnPos;
                np.xp = 0;
                myK.pieces.push_back(np);
                if (producedType == PieceType::Bishop) {
                    myK.recordSuccessfulBishopSpawnParity(
                        ProductionSpawnRules::squareColorParity(spawnPos));
                }
                b.isProducing = false;
            }
            // If no space, stays at turnsRemaining=0 and retries next turn
        }
    }

    processFriendlyRepairs(s, k, config);

    removeDestroyedStructures(myK);
    completeUnderConstructionBuildings(myK);

    const TurnEconomyBreakdown economyBreakdown = EconomySystem::calculateTurnEconomy(s, k, config);
    myK.gold = economyBreakdown.endingGold;

    SnapKingdom& enemyK = s.enemyKingdom(k);

    // 3. Arena XP
    for (auto& b : s.publicBuildings) {
        if (b.type != BuildingType::Arena) continue;
        auto cells = b.getOccupiedCells();

        bool myPresence = false, enemyPresence = false;
        for (auto& c : cells) {
            if (myK.getPieceAt(c)) myPresence = true;
            if (enemyK.getPieceAt(c)) enemyPresence = true;
        }
        if (myPresence && !enemyPresence) {
            for (auto& c : cells) {
                SnapPiece* p = myK.getPieceAt(c);
                if (p) {
                    p->xp += XPSystem::sampleArenaXP(s.xpSystemState, s.worldSeed, config);
                }
            }
        }
    }

    const SnapKingdom& nextKingdom = s.kingdom(opponent(k));
    s.turnBudget(opponent(k)) = toSnapTurnBudget(TurnPointRules::makeBudget(
        config,
        nextKingdom.movementPointsMaxBonus,
        nextKingdom.buildPointsMaxBonus));

}

// ========================================================================
// Threat map and check/checkmate on snapshots
// ========================================================================

ThreatMap ForwardModel::buildThreatMap(const GameSnapshot& s, KingdomId attacker,
                                        int globalMaxRange) {
    ThreatMap tm;
    const SnapKingdom& atk = s.kingdom(attacker);
    auto getAttackSquares = [&](const SnapPiece& piece) {
        std::vector<sf::Vector2i> attackSquares;

        switch (piece.type) {
            case PieceType::Pawn:
                attackSquares = getPawnThreatenedSquares(piece, s);
                break;
            case PieceType::Knight:
                attackSquares = getKnightMoves(piece, s);
                break;
            case PieceType::Bishop:
                for (const auto& direction : std::vector<std::pair<int, int>>{{-1, -1}, {-1, 1}, {1, -1}, {1, 1}}) {
                    auto directionalMoves = getDirectionalMoves(piece, s, direction.first, direction.second, globalMaxRange);
                    attackSquares.insert(attackSquares.end(), directionalMoves.begin(), directionalMoves.end());
                }
                break;
            case PieceType::Rook:
                for (const auto& direction : std::vector<std::pair<int, int>>{{0, -1}, {0, 1}, {-1, 0}, {1, 0}}) {
                    auto directionalMoves = getDirectionalMoves(piece, s, direction.first, direction.second, globalMaxRange);
                    attackSquares.insert(attackSquares.end(), directionalMoves.begin(), directionalMoves.end());
                }
                break;
            case PieceType::Queen:
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) {
                            continue;
                        }

                        auto directionalMoves = getDirectionalMoves(piece, s, dx, dy, globalMaxRange);
                        attackSquares.insert(attackSquares.end(), directionalMoves.begin(), directionalMoves.end());
                    }
                }
                break;
            case PieceType::King:
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) {
                            continue;
                        }

                        sf::Vector2i destination{piece.position.x + dx, piece.position.y + dy};
                        if (canLandOn(s, destination, piece.kingdom)) {
                            attackSquares.push_back(destination);
                        }
                    }
                }
                break;
        }

        return attackSquares;
    };

    for (auto& piece : atk.pieces) {
        const auto attackSquares = getAttackSquares(piece);
        for (const auto& square : attackSquares) {
            tm.mark(square);
        }
    }
    return tm;
}

bool ForwardModel::isInCheck(const GameSnapshot& s, KingdomId k, int globalMaxRange) {
    const SnapPiece* king = s.kingdom(k).getKing();
    if (!king) return false;

    KingdomId enemy = (k == KingdomId::White) ? KingdomId::Black : KingdomId::White;
    ThreatMap threats = buildThreatMap(s, enemy, globalMaxRange);
    return threats.isSet(king->position);
}

bool ForwardModel::isCheckmate(const GameSnapshot& s, KingdomId k, int globalMaxRange) {
    if (!isInCheck(s, k, globalMaxRange)) return false;

    const SnapKingdom& myK = s.kingdom(k);
    // Try every piece's every move — if any removes check, not checkmate
    for (auto& piece : myK.pieces) {
        auto moves = getLegalMoves(s, piece, globalMaxRange);
        for (auto& dest : moves) {
            GameSnapshot sim = s.clone();
            // Apply move in simulation
            SnapPiece* simPiece = sim.kingdom(k).getPieceById(piece.id);
            if (!simPiece) continue;

            // Capture enemy if present
            SnapKingdom& simEnemy = sim.enemyKingdom(k);
            SnapPiece* victim = simEnemy.getPieceAt(dest);
            if (victim) {
                if (victim->type == PieceType::King) continue; // can't capture king
                simEnemy.removePiece(victim->id);
            }
            simPiece->position = dest;

            if (!isInCheck(sim, k, globalMaxRange))
                return false; // found an escape
        }
    }
    return true; // no escape = checkmate
}
