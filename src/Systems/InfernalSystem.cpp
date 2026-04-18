#include "Systems/InfernalSystem.hpp"

#include <algorithm>
#include <limits>
#include <numeric>
#include <optional>
#include <functional>
#include <queue>
#include <random>
#include <vector>

#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Runtime/WeatherVisibility.hpp"
#include "Systems/StructureIntegrityRules.hpp"
#include "Units/MovementRules.hpp"
#include "Units/Piece.hpp"

namespace {

constexpr int kLambdaScale = 1000;
constexpr int kCardinalDirections[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

struct TargetCandidate {
    int pieceId = -1;
    PieceType type = PieceType::Pawn;
    sf::Vector2i position{0, 0};
};

struct SpawnOption {
    int targetPieceId = -1;
    PieceType targetType = PieceType::Pawn;
    sf::Vector2i spawnCell{0, 0};
    int weight = 0;
};

bool isVisibleToInfernal(const WeatherMaskCache& weatherMaskCache, sf::Vector2i position) {
    return !WeatherVisibility::cellHasConcealingFog(weatherMaskCache, position);
}

int& debtForKingdom(InfernalSystemState& state, KingdomId kingdom) {
    return (kingdom == KingdomId::White) ? state.whiteBloodDebt : state.blackBloodDebt;
}

std::uint32_t mixSeed(std::uint32_t seed, std::uint32_t value) {
    std::uint32_t mixed = seed ^ (value + 0x9e3779b9u + (seed << 6) + (seed >> 2));
    mixed ^= mixed >> 16;
    mixed *= 0x7feb352du;
    mixed ^= mixed >> 15;
    mixed *= 0x846ca68bu;
    mixed ^= mixed >> 16;
    return mixed;
}

std::mt19937 makeEventGenerator(InfernalSystemState& state, std::uint32_t worldSeed) {
    const std::uint32_t baseSeed = (worldSeed == 0) ? 1u : worldSeed;
    return std::mt19937(mixSeed(baseSeed, state.rngCounter++));
}

int cadenceStepsFromTurns(int turns) {
    return std::max(1, turns * 2);
}

bool isTargetablePieceType(PieceType type) {
    return type != PieceType::King;
}

bool isBorderCell(const Board& board, sf::Vector2i position) {
    if (!board.isInBounds(position.x, position.y)) {
        return false;
    }

    const Cell& cell = board.getCell(position.x, position.y);
    if (!cell.isInCircle) {
        return false;
    }

    for (const auto& direction : kCardinalDirections) {
        const int nx = position.x + direction[0];
        const int ny = position.y + direction[1];
        if (!board.isInBounds(nx, ny) || !board.getCell(nx, ny).isInCircle) {
            return true;
        }
    }

    return false;
}

bool isValidBorderSpawnCell(const Board& board, sf::Vector2i position) {
    if (!isBorderCell(board, position)) {
        return false;
    }

    const Cell& cell = board.getCell(position.x, position.y);
    return cell.type != CellType::Water
        && cell.building == nullptr
        && cell.piece == nullptr
        && cell.mapObject == nullptr
        && cell.autonomousUnit == nullptr;
}

bool isValidBorderReturnCell(const Board& board,
                             sf::Vector2i position,
                             sf::Vector2i currentPosition) {
    if (!isBorderCell(board, position)) {
        return false;
    }

    if (position == currentPosition) {
        return true;
    }

    return isValidBorderSpawnCell(board, position);
}

std::vector<sf::Vector2i> collectBorderCells(const Board& board,
                                             std::optional<sf::Vector2i> currentPosition = std::nullopt) {
    std::vector<sf::Vector2i> borderCells;
    for (const sf::Vector2i& cellPos : board.getAllValidCells()) {
        if (!currentPosition.has_value()) {
            if (isValidBorderSpawnCell(board, cellPos)) {
                borderCells.push_back(cellPos);
            }
            continue;
        }

        if (isValidBorderReturnCell(board, cellPos, *currentPosition)) {
            borderCells.push_back(cellPos);
        }
    }

    return borderCells;
}

Piece makeInfernalProxy(PieceType manifestedType,
                        KingdomId targetKingdom,
                        sf::Vector2i position) {
    return Piece(-1, manifestedType, opponent(targetKingdom), position);
}

std::vector<sf::Vector2i> generateInfernalMoves(PieceType manifestedType,
                                                KingdomId targetKingdom,
                                                sf::Vector2i position,
                                                const Board& board,
                                                const GameConfig& config) {
    Piece proxy = makeInfernalProxy(manifestedType, targetKingdom, position);
    std::vector<sf::Vector2i> moves = MovementRules::getValidMoves(proxy, board, config);
    moves.erase(
        std::remove_if(moves.begin(), moves.end(), [&](sf::Vector2i move) {
            if (!board.isInBounds(move.x, move.y)) {
                return true;
            }

            const Cell& cell = board.getCell(move.x, move.y);
            return cell.piece != nullptr && cell.piece->type == PieceType::King;
        }),
        moves.end());
    return moves;
}

int shortestPathSteps(PieceType manifestedType,
                      KingdomId targetKingdom,
                      sf::Vector2i start,
                      const Board& board,
                      const GameConfig& config,
                      const std::function<bool(sf::Vector2i)>& goalPredicate) {
    if (goalPredicate(start)) {
        return 0;
    }

    const int diameter = board.getDiameter();
    std::vector<int> distances(diameter * diameter, -1);
    std::queue<sf::Vector2i> frontier;

    distances[start.y * diameter + start.x] = 0;
    frontier.push(start);

    while (!frontier.empty()) {
        const sf::Vector2i current = frontier.front();
        frontier.pop();
        const int currentDistance = distances[current.y * diameter + current.x];

        for (const sf::Vector2i& move : generateInfernalMoves(
                 manifestedType,
                 targetKingdom,
                 current,
                 board,
                 config)) {
            const int index = move.y * diameter + move.x;
            if (distances[index] >= 0) {
                continue;
            }

            distances[index] = currentDistance + 1;
            if (goalPredicate(move)) {
                return distances[index];
            }

            frontier.push(move);
        }
    }

    return -1;
}

std::vector<TargetCandidate> collectTargetCandidates(const Kingdom& kingdom) {
    std::vector<TargetCandidate> candidates;
    candidates.reserve(kingdom.pieces.size());
    for (const Piece& piece : kingdom.pieces) {
        if (!isTargetablePieceType(piece.type)) {
            continue;
        }

        candidates.push_back(TargetCandidate{piece.id, piece.type, piece.position});
    }

    return candidates;
}

std::vector<TargetCandidate> collectVisibleTargetCandidates(const Kingdom& kingdom,
                                                           const WeatherMaskCache& weatherMaskCache) {
    std::vector<TargetCandidate> candidates;
    candidates.reserve(kingdom.pieces.size());
    for (const Piece& piece : kingdom.pieces) {
        if (!isTargetablePieceType(piece.type) || !isVisibleToInfernal(weatherMaskCache, piece.position)) {
            continue;
        }

        candidates.push_back(TargetCandidate{piece.id, piece.type, piece.position});
    }

    return candidates;
}

bool kingdomHasTargetablePieces(const Kingdom& kingdom) {
    return std::any_of(kingdom.pieces.begin(), kingdom.pieces.end(), [](const Piece& piece) {
        return isTargetablePieceType(piece.type);
    });
}

std::optional<PieceType> chooseTargetTypeFromCandidates(InfernalSystemState& state,
                                                        const std::vector<TargetCandidate>& targets,
                                                        std::uint32_t worldSeed,
                                                        const GameConfig& config) {
    if (targets.empty()) {
        return std::nullopt;
    }

    constexpr std::array<PieceType, 5> candidateTypes{
        PieceType::Queen,
        PieceType::Rook,
        PieceType::Bishop,
        PieceType::Knight,
        PieceType::Pawn};

    std::vector<int> typeWeights;
    typeWeights.reserve(candidateTypes.size());
    for (PieceType type : candidateTypes) {
        const bool hasType = std::any_of(targets.begin(), targets.end(), [type](const TargetCandidate& target) {
            return target.type == type;
        });
        typeWeights.push_back(hasType ? config.getInfernalTargetWeight(type) : 0);
    }

    if (std::accumulate(typeWeights.begin(), typeWeights.end(), 0) <= 0) {
        return targets.front().type;
    }

    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    std::discrete_distribution<std::size_t> typeDistribution(typeWeights.begin(), typeWeights.end());
    return candidateTypes[typeDistribution(generator)];
}

std::optional<sf::Vector2i> chooseRandomBorderSpawnCell(InfernalSystemState& state,
                                                        const Board& board,
                                                        std::uint32_t worldSeed) {
    std::vector<sf::Vector2i> borderCells = collectBorderCells(board);
    if (borderCells.empty()) {
        return std::nullopt;
    }

    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    std::uniform_int_distribution<std::size_t> distribution(0, borderCells.size() - 1);
    return borderCells[distribution(generator)];
}

void enterSearchingPhase(AutonomousUnit& unit) {
    unit.infernal.phase = InfernalPhase::Searching;
    unit.infernal.targetPieceId = -1;
}

std::optional<SpawnOption> chooseSpawnOptionForTargets(InfernalSystemState& state,
                                                       const Board& board,
                                                       const std::vector<TargetCandidate>& allTargets,
                                                       KingdomId targetKingdom,
                                                       std::uint32_t worldSeed,
                                                       const GameConfig& config) {
    if (allTargets.empty()) {
        return std::nullopt;
    }

    std::vector<sf::Vector2i> borderCells = collectBorderCells(board);
    if (borderCells.empty()) {
        return std::nullopt;
    }

    std::array<PieceType, 5> remainingTypes{
        PieceType::Queen,
        PieceType::Rook,
        PieceType::Bishop,
        PieceType::Knight,
        PieceType::Pawn};
    std::size_t remainingCount = remainingTypes.size();
    std::mt19937 generator = makeEventGenerator(state, worldSeed);

    while (remainingCount > 0) {
        std::vector<int> typeWeights;
        typeWeights.reserve(remainingCount);
        for (std::size_t index = 0; index < remainingCount; ++index) {
            const PieceType type = remainingTypes[index];
            const bool hasType = std::any_of(allTargets.begin(), allTargets.end(), [type](const TargetCandidate& target) {
                return target.type == type;
            });
            typeWeights.push_back(hasType ? config.getInfernalTargetWeight(type) : 0);
        }

        if (std::accumulate(typeWeights.begin(), typeWeights.end(), 0) <= 0) {
            return std::nullopt;
        }

        std::discrete_distribution<std::size_t> typeDistribution(typeWeights.begin(), typeWeights.end());
        const std::size_t chosenTypeIndex = typeDistribution(generator);
        const PieceType chosenType = remainingTypes[chosenTypeIndex];

        std::vector<SpawnOption> spawnOptions;
        for (const TargetCandidate& target : allTargets) {
            if (target.type != chosenType) {
                continue;
            }

            for (const sf::Vector2i& borderCell : borderCells) {
                const int distance = shortestPathSteps(
                    chosenType,
                    targetKingdom,
                    borderCell,
                    board,
                    config,
                    [&](sf::Vector2i position) {
                        return position == target.position;
                    });
                if (distance < 0) {
                    continue;
                }

                const int weight = std::max(1, (board.getDiameter() * 2) - distance + 1);
                spawnOptions.push_back(SpawnOption{target.pieceId, chosenType, borderCell, weight});
            }
        }

        if (!spawnOptions.empty()) {
            std::vector<int> optionWeights;
            optionWeights.reserve(spawnOptions.size());
            for (const SpawnOption& option : spawnOptions) {
                optionWeights.push_back(option.weight);
            }

            std::discrete_distribution<std::size_t> optionDistribution(optionWeights.begin(), optionWeights.end());
            return spawnOptions[optionDistribution(generator)];
        }

        remainingTypes[chosenTypeIndex] = remainingTypes[remainingCount - 1];
        --remainingCount;
    }

    return std::nullopt;
}

std::optional<KingdomId> chooseTargetKingdom(InfernalSystemState& state,
                                             const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                             std::uint32_t worldSeed) {
    const bool whiteEligible = kingdomHasTargetablePieces(kingdoms[kingdomIndex(KingdomId::White)]);
    const bool blackEligible = kingdomHasTargetablePieces(kingdoms[kingdomIndex(KingdomId::Black)]);
    if (!whiteEligible && !blackEligible) {
        return std::nullopt;
    }
    if (whiteEligible && !blackEligible) {
        return KingdomId::White;
    }
    if (!whiteEligible && blackEligible) {
        return KingdomId::Black;
    }

    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    const int totalDebt = state.whiteBloodDebt + state.blackBloodDebt;
    const double whiteProbability = (totalDebt > 0)
        ? static_cast<double>(state.whiteBloodDebt) / static_cast<double>(totalDebt)
        : 0.5;
    std::bernoulli_distribution distribution(std::clamp(whiteProbability, 0.0, 1.0));
    return distribution(generator) ? KingdomId::White : KingdomId::Black;
}

std::optional<TargetCandidate> chooseReplacementTarget(InfernalSystemState& state,
                                                       const Board& board,
                                                       const Kingdom& targetKingdomState,
                                                       const AutonomousUnit& unit,
                                                       const WeatherMaskCache& weatherMaskCache,
                                                       std::uint32_t worldSeed,
                                                       const GameConfig& config) {
    std::vector<TargetCandidate> targets = collectVisibleTargetCandidates(targetKingdomState, weatherMaskCache);
    if (targets.empty()) {
        return std::nullopt;
    }

    std::array<PieceType, 5> remainingTypes{
        PieceType::Queen,
        PieceType::Rook,
        PieceType::Bishop,
        PieceType::Knight,
        PieceType::Pawn};
    std::size_t remainingCount = remainingTypes.size();
    std::mt19937 generator = makeEventGenerator(state, worldSeed);

    while (remainingCount > 0) {
        std::vector<int> typeWeights;
        typeWeights.reserve(remainingCount);
        for (std::size_t index = 0; index < remainingCount; ++index) {
            const PieceType type = remainingTypes[index];
            const bool hasType = std::any_of(targets.begin(), targets.end(), [type](const TargetCandidate& candidate) {
                return candidate.type == type;
            });
            int weight = hasType ? config.getInfernalTargetWeight(type) : 0;
            if (type == unit.infernal.preferredTargetType && weight > 0) {
                weight += weight;
            }
            typeWeights.push_back(weight);
        }

        if (std::accumulate(typeWeights.begin(), typeWeights.end(), 0) <= 0) {
            return std::nullopt;
        }

        std::discrete_distribution<std::size_t> typeDistribution(typeWeights.begin(), typeWeights.end());
        const std::size_t chosenTypeIndex = typeDistribution(generator);
        const PieceType chosenType = remainingTypes[chosenTypeIndex];

        std::vector<TargetCandidate> reachableTargets;
        std::vector<int> reachableWeights;
        for (const TargetCandidate& target : targets) {
            if (target.type != chosenType) {
                continue;
            }

            const int distance = shortestPathSteps(
                unit.infernal.manifestedPieceType,
                unit.infernal.targetKingdom,
                unit.position,
                board,
                config,
                [&](sf::Vector2i position) {
                    return position == target.position;
                });
            if (distance < 0) {
                continue;
            }

            reachableTargets.push_back(target);
            reachableWeights.push_back(std::max(1, (board.getDiameter() * 2) - distance + 1));
        }

        if (!reachableTargets.empty()) {
            std::discrete_distribution<std::size_t> targetDistribution(
                reachableWeights.begin(),
                reachableWeights.end());
            return reachableTargets[targetDistribution(generator)];
        }

        remainingTypes[chosenTypeIndex] = remainingTypes[remainingCount - 1];
        --remainingCount;
    }

    return std::nullopt;
}

std::optional<sf::Vector2i> chooseSearchingRandomMove(InfernalSystemState& state,
                                                      const Board& board,
                                                      const AutonomousUnit& unit,
                                                      const WeatherMaskCache& weatherMaskCache,
                                                      std::uint32_t worldSeed,
                                                      const GameConfig& config) {
    const int chanceTimes1000 = std::clamp(
        config.getInfernalSearchingRandomMoveChanceTimes1000(),
        0,
        kLambdaScale);
    if (chanceTimes1000 <= 0) {
        return std::nullopt;
    }

    std::vector<sf::Vector2i> moves = generateInfernalMoves(
        unit.infernal.manifestedPieceType,
        unit.infernal.targetKingdom,
        unit.position,
        board,
        config);
    moves.erase(
        std::remove_if(moves.begin(), moves.end(), [&](sf::Vector2i move) {
            const Cell& cell = board.getCell(move.x, move.y);
            return cell.piece != nullptr
                && cell.piece->kingdom == unit.infernal.targetKingdom
                && !isVisibleToInfernal(weatherMaskCache, cell.piece->position);
        }),
        moves.end());
    if (moves.empty()) {
        return std::nullopt;
    }

    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    if (chanceTimes1000 < kLambdaScale) {
        std::uniform_int_distribution<int> chanceDistribution(0, kLambdaScale - 1);
        if (chanceDistribution(generator) >= chanceTimes1000) {
            return std::nullopt;
        }
    }

    std::uniform_int_distribution<std::size_t> moveDistribution(0, moves.size() - 1);
    return moves[moveDistribution(generator)];
}

bool applyInfernalArrivalEffects(Board& board,
                                 Kingdom& targetKingdom,
                                 const AutonomousUnit& unit,
                                 sf::Vector2i destination,
                                 const GameConfig& config,
                                 int trackedTargetPieceId) {
    Cell& destinationCell = board.getCell(destination.x, destination.y);
    const bool capturedTrackedTarget = destinationCell.piece != nullptr
        && destinationCell.piece->kingdom == unit.infernal.targetKingdom
        && destinationCell.piece->id == trackedTargetPieceId;
    if (destinationCell.piece != nullptr
        && destinationCell.piece->kingdom == unit.infernal.targetKingdom
        && isTargetablePieceType(destinationCell.piece->type)) {
        const int capturedPieceId = destinationCell.piece->id;
        destinationCell.piece = nullptr;
        targetKingdom.removePiece(capturedPieceId);
    }

    if (destinationCell.building != nullptr
        && !destinationCell.building->isNeutral
        && destinationCell.building->owner == unit.infernal.targetKingdom) {
        const int localX = destination.x - destinationCell.building->origin.x;
        const int localY = destination.y - destinationCell.building->origin.y;
        const StructureOccupancyResult occupancyResult = StructureIntegrityRules::applyEnemyOccupancy(
            *destinationCell.building,
            localX,
            localY,
            config);
        if (occupancyResult != StructureOccupancyResult::None
            && destinationCell.building->isDestroyed()
            && StructureIntegrityRules::shouldRemoveWhenFullyDestroyed(destinationCell.building->type)) {
            const int buildingId = destinationCell.building->id;
            targetKingdom.removeBuilding(buildingId);
        }
    }

    return capturedTrackedTarget;
}

std::optional<AutonomousUnit> spawnInfernalFromResolvedTarget(InfernalSystemState& state,
                                                              const Board& board,
                                                              const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                                              const WeatherMaskCache& weatherMaskCache,
                                                              std::uint32_t worldSeed,
                                                              int currentTurnStep,
                                                              int currentTurnNumber,
                                                              int nextUnitId,
                                                              const GameConfig& config) {
    const std::optional<KingdomId> primaryTargetKingdom = chooseTargetKingdom(state, kingdoms, worldSeed);
    if (!primaryTargetKingdom.has_value()) {
        state.nextSpawnTurn = currentTurnStep + cadenceStepsFromTurns(config.getInfernalSpawnRetryTurns());
        return std::nullopt;
    }

    const Kingdom& chosenKingdom = kingdoms[kingdomIndex(*primaryTargetKingdom)];
    const std::vector<TargetCandidate> allTargets = collectTargetCandidates(chosenKingdom);
    if (allTargets.empty()) {
        state.nextSpawnTurn = currentTurnStep + cadenceStepsFromTurns(config.getInfernalSpawnRetryTurns());
        return std::nullopt;
    }

    const std::vector<TargetCandidate> visibleTargets = collectVisibleTargetCandidates(
        chosenKingdom,
        weatherMaskCache);
    std::optional<SpawnOption> option = chooseSpawnOptionForTargets(
        state,
        board,
        visibleTargets,
        *primaryTargetKingdom,
        worldSeed,
        config);

    AutonomousUnit unit;
    unit.id = nextUnitId;
    unit.type = AutonomousUnitType::InfernalPiece;
    unit.infernal.targetKingdom = *primaryTargetKingdom;

    if (option.has_value()) {
        unit.position = option->spawnCell;
        unit.infernal.targetPieceId = option->targetPieceId;
        unit.infernal.manifestedPieceType = option->targetType;
        unit.infernal.preferredTargetType = option->targetType;
        unit.infernal.phase = InfernalPhase::Hunting;
        unit.infernal.returnBorderCell = option->spawnCell;
    } else {
        const std::optional<sf::Vector2i> spawnCell = chooseRandomBorderSpawnCell(state, board, worldSeed);
        if (!spawnCell.has_value()) {
            state.nextSpawnTurn = currentTurnStep + cadenceStepsFromTurns(config.getInfernalSpawnRetryTurns());
            return std::nullopt;
        }

        unit.position = *spawnCell;
        unit.infernal.targetPieceId = -1;
        unit.infernal.manifestedPieceType = chooseTargetTypeFromCandidates(
            state,
            allTargets,
            worldSeed,
            config).value_or(PieceType::Queen);
        unit.infernal.preferredTargetType = unit.infernal.manifestedPieceType;
        unit.infernal.phase = InfernalPhase::Searching;
        unit.infernal.returnBorderCell = *spawnCell;
    }
    unit.infernal.spawnTurn = currentTurnNumber;

    state.activeInfernalUnitId = unit.id;
    return unit;
}

std::optional<sf::Vector2i> chooseReturnBorderCell(InfernalSystemState& state,
                                                   const Board& board,
                                                   const AutonomousUnit& unit,
                                                   std::uint32_t worldSeed,
                                                   const GameConfig& config) {
    std::vector<sf::Vector2i> borderCells = collectBorderCells(board, unit.position);
    if (borderCells.empty()) {
        return std::nullopt;
    }

    std::vector<sf::Vector2i> bestCells;

    int bestDistance = std::numeric_limits<int>::max();
    for (const sf::Vector2i& borderCell : borderCells) {
        const int distance = shortestPathSteps(
            unit.infernal.manifestedPieceType,
            unit.infernal.targetKingdom,
            unit.position,
            board,
            config,
            [&](sf::Vector2i position) {
                return position == borderCell;
            });
        if (distance < 0) {
            continue;
        }

        if (distance < bestDistance) {
            bestDistance = distance;
            bestCells.clear();
            bestCells.push_back(borderCell);
        } else if (distance == bestDistance) {
            bestCells.push_back(borderCell);
        }
    }

    if (bestCells.empty()) {
        return std::nullopt;
    }

    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    std::uniform_int_distribution<std::size_t> distribution(0, bestCells.size() - 1);
    return bestCells[distribution(generator)];
}

bool moveInfernalTowardGoal(Board& board,
                            const GameConfig& config,
                            const std::function<int(sf::Vector2i)>& distanceFromMove,
                            AutonomousUnit& unit,
                            sf::Vector2i* chosenMove = nullptr) {
    std::vector<sf::Vector2i> moves = generateInfernalMoves(
        unit.infernal.manifestedPieceType,
        unit.infernal.targetKingdom,
        unit.position,
        board,
        config);
    if (moves.empty()) {
        return false;
    }

    int bestDistance = std::numeric_limits<int>::max();
    std::optional<sf::Vector2i> bestMove;
    for (const sf::Vector2i& move : moves) {
        const int distance = distanceFromMove(move);
        if (distance < 0) {
            continue;
        }

        const Cell& cell = board.getCell(move.x, move.y);
        const bool capturesTargetPiece = cell.piece != nullptr
            && cell.piece->kingdom == unit.infernal.targetKingdom
            && cell.piece->id == unit.infernal.targetPieceId;
        if (capturesTargetPiece) {
            bestMove = move;
            bestDistance = -1;
            break;
        }

        if (!bestMove.has_value() || distance < bestDistance) {
            bestMove = move;
            bestDistance = distance;
        }
    }

    if (!bestMove.has_value()) {
        return false;
    }

    if (chosenMove) {
        *chosenMove = *bestMove;
    }
    unit.position = *bestMove;
    return true;
}

void despawnInfernal(InfernalSystemState& state,
                     std::vector<AutonomousUnit>& units,
                     int unitId,
                     int currentTurnStep,
                     const GameConfig& config) {
    units.erase(
        std::remove_if(units.begin(), units.end(), [unitId](const AutonomousUnit& unit) {
            return unit.id == unitId;
        }),
        units.end());
    InfernalSystem::clearActiveInfernal(state);
    InfernalSystem::scheduleNextSpawn(state, currentTurnStep, config);
}

} // namespace

void InfernalSystem::initialize(InfernalSystemState& state,
                                int currentTurnStep,
                                const GameConfig& config) {
    state.activeInfernalUnitId = -1;
    state.whiteBloodDebt = 0;
    state.blackBloodDebt = 0;
    state.rngCounter = 0;
    state.nextSpawnTurn = std::max(currentTurnStep, cadenceStepsFromTurns(config.getInfernalMinSpawnTurn()));
}

void InfernalSystem::scheduleNextSpawn(InfernalSystemState& state,
                                       int currentTurnStep,
                                       const GameConfig& config) {
    state.activeInfernalUnitId = -1;
    state.nextSpawnTurn = currentTurnStep + cadenceStepsFromTurns(config.getInfernalRespawnCooldownTurns());
}

void InfernalSystem::onInfernalRemoved(InfernalSystemState& state,
                                       int currentTurnStep,
                                       const GameConfig& config) {
    clearActiveInfernal(state);
    scheduleNextSpawn(state, currentTurnStep, config);
}

void InfernalSystem::decayBloodDebt(InfernalSystemState& state, int decayPercent) {
    const int clampedPercent = std::clamp(decayPercent, 0, 100);
    state.whiteBloodDebt = (state.whiteBloodDebt * clampedPercent) / 100;
    state.blackBloodDebt = (state.blackBloodDebt * clampedPercent) / 100;
}

void InfernalSystem::addBloodDebtForCapturedPiece(InfernalSystemState& state,
                                                  KingdomId actorKingdom,
                                                  PieceType victimType,
                                                  int debtAmount) {
    if (!isTargetablePieceType(victimType) || debtAmount <= 0) {
        return;
    }

    debtForKingdom(state, actorKingdom) += debtAmount;
}

void InfernalSystem::addBloodDebtForStructureDamage(InfernalSystemState& state,
                                                    KingdomId actorKingdom,
                                                    int debtAmount) {
    if (debtAmount <= 0) {
        return;
    }

    debtForKingdom(state, actorKingdom) += debtAmount;
}

bool InfernalSystem::hasActiveInfernal(const InfernalSystemState& state) {
    return state.activeInfernalUnitId >= 0;
}

AutonomousUnit* InfernalSystem::findActiveInfernal(std::vector<AutonomousUnit>& units,
                                                   const InfernalSystemState& state) {
    if (!hasActiveInfernal(state)) {
        return nullptr;
    }

    for (AutonomousUnit& unit : units) {
        if (unit.id == state.activeInfernalUnitId) {
            return &unit;
        }
    }

    return nullptr;
}

const AutonomousUnit* InfernalSystem::findActiveInfernal(const std::vector<AutonomousUnit>& units,
                                                         const InfernalSystemState& state) {
    if (!hasActiveInfernal(state)) {
        return nullptr;
    }

    for (const AutonomousUnit& unit : units) {
        if (unit.id == state.activeInfernalUnitId) {
            return &unit;
        }
    }

    return nullptr;
}

void InfernalSystem::clearActiveInfernal(InfernalSystemState& state) {
    state.activeInfernalUnitId = -1;
}

std::optional<AutonomousUnit> InfernalSystem::trySpawnInfernal(
    InfernalSystemState& state,
    const Board& board,
    const std::array<Kingdom, kNumKingdoms>& kingdoms,
    const WeatherMaskCache& weatherMaskCache,
    std::uint32_t worldSeed,
    int currentTurnStep,
    int currentTurnNumber,
    int nextUnitId,
    const GameConfig& config) {
    if (hasActiveInfernal(state) || currentTurnStep < state.nextSpawnTurn) {
        return std::nullopt;
    }

    state.nextSpawnTurn = currentTurnStep + 1;
    if (currentTurnNumber < config.getInfernalMinSpawnTurn()) {
        return std::nullopt;
    }

    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    const double lambdaBase = static_cast<double>(config.getInfernalPoissonLambdaBaseTimes1000())
        / static_cast<double>(kLambdaScale);
    const double lambdaPerDebt = static_cast<double>(config.getInfernalPoissonLambdaPerDebtTimes1000())
        / static_cast<double>(kLambdaScale);
    const double lambdaCap = static_cast<double>(config.getInfernalPoissonLambdaCapTimes1000())
        / static_cast<double>(kLambdaScale);
    const double lambda = std::clamp(
        lambdaBase + lambdaPerDebt * static_cast<double>(state.whiteBloodDebt + state.blackBloodDebt),
        0.0,
        lambdaCap);
    std::poisson_distribution<int> distribution(lambda);
    if (distribution(generator) < 1) {
        return std::nullopt;
    }

    return spawnInfernalFromResolvedTarget(
        state,
        board,
        kingdoms,
        weatherMaskCache,
        worldSeed,
        currentTurnStep,
        currentTurnNumber,
        nextUnitId,
        config);
}

std::optional<AutonomousUnit> InfernalSystem::forceSpawnInfernal(
    InfernalSystemState& state,
    const Board& board,
    const std::array<Kingdom, kNumKingdoms>& kingdoms,
    const WeatherMaskCache& weatherMaskCache,
    std::uint32_t worldSeed,
    int currentTurnStep,
    int currentTurnNumber,
    int nextUnitId,
    const GameConfig& config) {
    if (hasActiveInfernal(state)) {
        return std::nullopt;
    }

    state.nextSpawnTurn = currentTurnStep + 1;
    return spawnInfernalFromResolvedTarget(
        state,
        board,
        kingdoms,
        weatherMaskCache,
        worldSeed,
        currentTurnStep,
        currentTurnNumber,
        nextUnitId,
        config);
}

bool InfernalSystem::actAfterCommittedTurn(InfernalSystemState& state,
                                           Board& board,
                                           std::array<Kingdom, kNumKingdoms>& kingdoms,
                                           std::vector<AutonomousUnit>& units,
                                           const WeatherMaskCache& weatherMaskCache,
                                           std::uint32_t worldSeed,
                                           int currentTurnStep,
                                           int currentTurnNumber,
                                           KingdomId justEndedKingdom,
                                           const GameConfig& config) {
    (void) currentTurnNumber;

    AutonomousUnit* unit = findActiveInfernal(units, state);
    if (unit == nullptr) {
        return false;
    }
    if (!shouldActAfterCommittedTurn(unit->infernal.targetKingdom, justEndedKingdom)) {
        return false;
    }

    if (board.isInBounds(unit->position.x, unit->position.y)) {
        Cell& currentCell = board.getCell(unit->position.x, unit->position.y);
        if (currentCell.autonomousUnit == unit) {
            currentCell.autonomousUnit = nullptr;
        }
    }

    bool changed = false;
    Kingdom& targetKingdom = kingdoms[kingdomIndex(unit->infernal.targetKingdom)];

    if (unit->infernal.phase == InfernalPhase::Searching) {
        const std::optional<TargetCandidate> replacement = chooseReplacementTarget(
            state,
            board,
            targetKingdom,
            *unit,
            weatherMaskCache,
            worldSeed,
            config);
        if (replacement.has_value()) {
            unit->infernal.targetPieceId = replacement->pieceId;
            unit->infernal.preferredTargetType = replacement->type;
            unit->infernal.phase = InfernalPhase::Hunting;
            changed = true;
        } else if (!kingdomHasTargetablePieces(targetKingdom)) {
            unit->infernal.phase = InfernalPhase::Returning;
            const std::optional<sf::Vector2i> returnCell = chooseReturnBorderCell(
                state,
                board,
                *unit,
                worldSeed,
                config);
            unit->infernal.returnBorderCell = returnCell.value_or(unit->position);
            changed = true;
        } else if (const std::optional<sf::Vector2i> randomMove = chooseSearchingRandomMove(
                       state,
                       board,
                       *unit,
                       weatherMaskCache,
                       worldSeed,
                       config)) {
            unit->position = *randomMove;
            applyInfernalArrivalEffects(
                board,
                targetKingdom,
                *unit,
                *randomMove,
                config,
                -1);
            changed = true;

            if (!kingdomHasTargetablePieces(targetKingdom)) {
                unit->infernal.phase = InfernalPhase::Returning;
                const std::optional<sf::Vector2i> returnCell = chooseReturnBorderCell(
                    state,
                    board,
                    *unit,
                    worldSeed,
                    config);
                unit->infernal.returnBorderCell = returnCell.value_or(unit->position);
            }
        }
    }

    if (unit->infernal.phase == InfernalPhase::Hunting) {
        const Piece* targetPiece = targetKingdom.getPieceById(unit->infernal.targetPieceId);
        const bool targetPieceVisible = targetPiece != nullptr
            && isTargetablePieceType(targetPiece->type)
            && isVisibleToInfernal(weatherMaskCache, targetPiece->position);
        if (!targetPieceVisible) {
            const std::optional<TargetCandidate> replacement = chooseReplacementTarget(
                state,
                board,
                targetKingdom,
                *unit,
                weatherMaskCache,
                worldSeed,
                config);
            if (replacement.has_value()) {
                unit->infernal.targetPieceId = replacement->pieceId;
                unit->infernal.preferredTargetType = replacement->type;
                targetPiece = targetKingdom.getPieceById(unit->infernal.targetPieceId);
                changed = true;
            } else if (kingdomHasTargetablePieces(targetKingdom)) {
                enterSearchingPhase(*unit);
                changed = true;
            } else {
                unit->infernal.phase = InfernalPhase::Returning;
                const std::optional<sf::Vector2i> returnCell = chooseReturnBorderCell(
                    state,
                    board,
                    *unit,
                    worldSeed,
                    config);
                unit->infernal.returnBorderCell = returnCell.value_or(unit->position);
                changed = true;
            }
        }

        if (unit->infernal.phase == InfernalPhase::Hunting && targetPiece != nullptr) {
            sf::Vector2i chosenMove{0, 0};
            const bool moved = moveInfernalTowardGoal(
                board,
                config,
                [&](sf::Vector2i move) {
                    if (move == targetPiece->position) {
                        return 0;
                    }

                    return shortestPathSteps(
                        unit->infernal.manifestedPieceType,
                        unit->infernal.targetKingdom,
                        move,
                        board,
                        config,
                        [&](sf::Vector2i position) {
                            return position == targetPiece->position;
                        });
                },
                *unit,
                &chosenMove);

            if (!moved) {
                const std::optional<TargetCandidate> replacement = chooseReplacementTarget(
                    state,
                    board,
                    targetKingdom,
                    *unit,
                    weatherMaskCache,
                    worldSeed,
                    config);
                if (replacement.has_value()) {
                    unit->infernal.targetPieceId = replacement->pieceId;
                    unit->infernal.preferredTargetType = replacement->type;
                    changed = true;
                } else if (kingdomHasTargetablePieces(targetKingdom)) {
                    enterSearchingPhase(*unit);
                    changed = true;
                } else {
                    unit->infernal.phase = InfernalPhase::Returning;
                    const std::optional<sf::Vector2i> returnCell = chooseReturnBorderCell(
                        state,
                        board,
                        *unit,
                        worldSeed,
                        config);
                    if (!returnCell.has_value()) {
                        const int unitId = unit->id;
                        despawnInfernal(state, units, unitId, currentTurnStep, config);
                        return true;
                    }

                    unit->infernal.returnBorderCell = *returnCell;
                    changed = true;
                }
            } else {
                const int targetPieceId = unit->infernal.targetPieceId;
                const bool capturedTrackedTarget = applyInfernalArrivalEffects(
                    board,
                    targetKingdom,
                    *unit,
                    chosenMove,
                    config,
                    targetPieceId);

                changed = true;

                if (capturedTrackedTarget) {
                    const std::optional<TargetCandidate> replacement = chooseReplacementTarget(
                        state,
                        board,
                        targetKingdom,
                        *unit,
                        weatherMaskCache,
                        worldSeed,
                        config);
                    if (replacement.has_value()) {
                        unit->infernal.targetPieceId = replacement->pieceId;
                        unit->infernal.preferredTargetType = replacement->type;
                    } else if (kingdomHasTargetablePieces(targetKingdom)) {
                        enterSearchingPhase(*unit);
                    } else {
                        unit->infernal.phase = InfernalPhase::Returning;
                        const std::optional<sf::Vector2i> returnCell = chooseReturnBorderCell(
                            state,
                            board,
                            *unit,
                            worldSeed,
                            config);
                        if (!returnCell.has_value()) {
                            const int unitId = unit->id;
                            despawnInfernal(state, units, unitId, currentTurnStep, config);
                            return true;
                        }

                        unit->infernal.returnBorderCell = *returnCell;
                    }
                }
            }
        }
    }

    if (unit->infernal.phase == InfernalPhase::Returning) {
        if (isBorderCell(board, unit->position)) {
            const int unitId = unit->id;
            despawnInfernal(state, units, unitId, currentTurnStep, config);
            return true;
        }

        if (!isValidInfernalReturnBorderCell(unit->infernal.returnBorderCell)) {
            const std::optional<sf::Vector2i> returnCell = chooseReturnBorderCell(
                state,
                board,
                *unit,
                worldSeed,
                config);
            if (!returnCell.has_value()) {
                const int unitId = unit->id;
                despawnInfernal(state, units, unitId, currentTurnStep, config);
                return true;
            }

            unit->infernal.returnBorderCell = *returnCell;
            changed = true;
        }

        sf::Vector2i chosenMove{0, 0};
        const bool moved = moveInfernalTowardGoal(
            board,
            config,
            [&](sf::Vector2i move) {
                return shortestPathSteps(
                    unit->infernal.manifestedPieceType,
                    unit->infernal.targetKingdom,
                    move,
                    board,
                    config,
                    [&](sf::Vector2i position) {
                        return position == unit->infernal.returnBorderCell;
                    });
            },
            *unit,
            &chosenMove);
        if (!moved) {
            const int unitId = unit->id;
            despawnInfernal(state, units, unitId, currentTurnStep, config);
            return true;
        }

        changed = true;
        if (isBorderCell(board, unit->position)) {
            const int unitId = unit->id;
            despawnInfernal(state, units, unitId, currentTurnStep, config);
            return true;
        }
    }

    return changed;
}

bool InfernalSystem::shouldActAfterCommittedTurn(KingdomId targetKingdom, KingdomId justEndedKingdom) {
    return targetKingdom != justEndedKingdom;
}