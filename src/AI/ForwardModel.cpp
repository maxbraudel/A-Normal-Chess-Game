#include "AI/ForwardModel.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Buildings/Building.hpp"
#include "Units/Piece.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/BuildReachRules.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/ProductionSpawnRules.hpp"
#include "Systems/StructureIntegrityRules.hpp"
#include <cmath>
#include <algorithm>

namespace {

bool isBlockingWallCell(const SnapBuilding* building, sf::Vector2i pos) {
    if (!building) {
        return false;
    }

    const int localX = pos.x - building->origin.x;
    const int localY = pos.y - building->origin.y;
    return StructureIntegrityRules::isWallCellBlocking(*building, localX, localY);
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
            piece.xp += config.getDestroyBlockXP();
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
                                          int turnNumber) {
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
    static const int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    for (auto& d : dirs) {
        sf::Vector2i dest{piece.position.x + d[0], piece.position.y + d[1]};
        if (canLandOn(s, dest, piece.kingdom))
            moves.push_back(dest);
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
                              KingdomId mover) {
    SnapKingdom& myK = s.kingdom(mover);
    SnapPiece* piece = myK.getPieceById(pieceId);
    if (!piece) return false;

    // Capture enemy piece at destination
    SnapKingdom& enemyK = s.enemyKingdom(mover);
    SnapPiece* victim = enemyK.getPieceAt(dest);
    if (victim) {
        if (victim->type == PieceType::King) return false; // illegal
        // Grant XP to attacker (simplified)
        piece->xp += 20; // base XP for any capture
        enemyK.removePiece(victim->id);
    }

    piece->position = dest;
    return true;
}

bool ForwardModel::applyBuild(GameSnapshot& s, KingdomId k, BuildingType type,
                               sf::Vector2i pos, int width, int height,
                               int cost, int cellHPValue) {
    SnapKingdom& myK = s.kingdom(k);
    if (myK.gold < cost) return false;

    // Validate space
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            int cx = pos.x + dx, cy = pos.y + dy;
            if (!s.isTraversable(cx, cy)) return false;
            if (s.pieceAt({cx, cy})) return false;
            if (s.buildingAt({cx, cy})) return false;
        }
    }

    if (!footprintHasAdjacentBuilder(pos, width, height,
                                     collectBuilderPositions(myK.pieces))) {
        return false;
    }

    myK.gold -= cost;

    SnapBuilding bld;
    static int nextSnapBuildId = 90000;
    bld.id = nextSnapBuildId++;
    bld.type = type;
    bld.owner = k;
    bld.isNeutral = false;
    bld.origin = pos;
    bld.width = width;
    bld.height = height;
    bld.cellHP.assign(width * height, cellHPValue);
    bld.cellBreachState.assign(width * height, 0);
    myK.buildings.push_back(bld);
    return true;
}

bool ForwardModel::applyProduce(GameSnapshot& s, int barracksId, PieceType type,
                                 int cost, int productionTurns, KingdomId k) {
    SnapKingdom& myK = s.kingdom(k);
    SnapBuilding* barracks = myK.getBuildingById(barracksId);
    if (!barracks || barracks->isProducing || barracks->isDestroyed()) return false;
    if (myK.gold < cost) return false;

    myK.gold -= cost;
    barracks->isProducing = true;
    barracks->producingType = type;
    barracks->turnsRemaining = productionTurns;
    return true;
}

bool ForwardModel::applyMarriage(GameSnapshot& s, KingdomId k) {
    SnapKingdom& myK = s.kingdom(k);
    if (myK.hasQueen()) return false;

    // Find church
    const SnapBuilding* church = nullptr;
    for (auto& b : s.publicBuildings) {
        if (b.type == BuildingType::Church) { church = &b; break; }
    }
    if (!church) return false;

    auto cells = church->getOccupiedCells();

    // Check no enemy pieces on church
    SnapKingdom& enemyK = s.enemyKingdom(k);
    for (auto& cell : cells) {
        if (enemyK.getPieceAt(cell)) return false;
    }

    // Find king, bishop, pawn on church
    SnapPiece* king = nullptr;
    SnapPiece* bishop = nullptr;
    SnapPiece* pawn = nullptr;
    for (auto& p : myK.pieces) {
        bool onChurch = false;
        for (auto& c : cells) if (p.position == c) { onChurch = true; break; }
        if (!onChurch) continue;
        if (p.type == PieceType::King) king = &p;
        else if (p.type == PieceType::Bishop && !bishop) bishop = &p;
        else if (p.type == PieceType::Pawn && !pawn) pawn = &p;
    }

    if (!king || !bishop || !pawn) return false;

    // Check king-pawn adjacency
    int dx = std::abs(king->position.x - pawn->position.x);
    int dy = std::abs(king->position.y - pawn->position.y);
    if (dx > 1 || dy > 1) return false;

    // Transform pawn into queen
    pawn->type = PieceType::Queen;
    pawn->xp = 0;
    return true;
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
                               int arenaXP, const GameConfig& config) {
    SnapKingdom& myK = s.kingdom(k);

    processEnemyStructureOccupancy(s, k, config);

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

    // 2. Collect income from net occupation advantage on public resources.
    SnapKingdom& enemyK = s.enemyKingdom(k);
    for (auto& b : s.publicBuildings) {
        if (b.type != BuildingType::Mine && b.type != BuildingType::Farm) continue;
        auto cells = b.getOccupiedCells();

        int myOccupiedCells = 0;
        int enemyOccupiedCells = 0;
        for (auto& c : cells) {
            if (myK.getPieceAt(c)) ++myOccupiedCells;
            if (enemyK.getPieceAt(c)) ++enemyOccupiedCells;
        }

        const int incomePerCell = (b.type == BuildingType::Mine) ? mineIncomePerCell : farmIncomePerCell;
        const ResourceIncomeBreakdown breakdown = (k == KingdomId::White)
            ? EconomySystem::calculateResourceIncomeFromOccupation(myOccupiedCells, enemyOccupiedCells, incomePerCell)
            : EconomySystem::calculateResourceIncomeFromOccupation(enemyOccupiedCells, myOccupiedCells, incomePerCell);
        myK.gold += breakdown.incomeFor(k);
    }

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
                if (p) p->xp += arenaXP;
            }
        }
    }

}

// ========================================================================
// Threat map and check/checkmate on snapshots
// ========================================================================

ThreatMap ForwardModel::buildThreatMap(const GameSnapshot& s, KingdomId attacker,
                                        int globalMaxRange) {
    ThreatMap tm;
    const SnapKingdom& atk = s.kingdom(attacker);
    for (auto& piece : atk.pieces) {
        auto moves = getPseudoLegalMoves(s, piece, globalMaxRange);
        for (auto& m : moves) tm.mark(m);
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
