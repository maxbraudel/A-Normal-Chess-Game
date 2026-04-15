#include "Board/BoardGenerator.hpp"

#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Board/CellType.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>

namespace {

constexpr float kPi = 3.14159265f;
constexpr int kNeighbourDx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
constexpr int kNeighbourDy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr int kFlipHorizontalMask = 1;
constexpr int kFlipVerticalMask = 2;

struct TerrainComponent {
    std::vector<sf::Vector2i> cells;
    float averageScore = 0.f;
};

int clampInt(int value, int minValue, int maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

int toIndex(int x, int y, int diameter) {
    return (y * diameter) + x;
}

int regionCellCountFromRadius(int radius) {
    const int clampedRadius = std::max(1, radius);
    return std::max(1, static_cast<int>(std::round(kPi * clampedRadius * clampedRadius)));
}

float smoothStep(float value) {
    return value * value * (3.f - (2.f * value));
}

std::uint32_t mixSeed(std::uint32_t seed, std::uint32_t value) {
    std::uint32_t hash = seed ^ (value + 0x9e3779b9u + (seed << 6) + (seed >> 2));
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16;
    return hash;
}

float hashValue(std::uint32_t seed, int x, int y) {
    const auto hashed = mixSeed(seed, static_cast<std::uint32_t>(x * 374761393) ^ static_cast<std::uint32_t>(y * 668265263));
    return static_cast<float>(hashed & 0x00ffffffu) / static_cast<float>(0x00ffffffu);
}

int terrainFlipMaskFor(std::uint32_t worldSeed, int x, int y, CellType type) {
    if (type == CellType::Void) {
        return 0;
    }

    std::uint32_t seed = (worldSeed == 0) ? 1u : worldSeed;
    seed = mixSeed(seed, static_cast<std::uint32_t>(type) + 1u);
    const std::uint32_t positionHash = (static_cast<std::uint32_t>(x + 1) * 73856093u)
        ^ (static_cast<std::uint32_t>(y + 1) * 19349663u);
    seed = mixSeed(seed, positionHash);
    return static_cast<int>(seed & (kFlipHorizontalMask | kFlipVerticalMask));
}

float lerp(float a, float b, float t) {
    return a + ((b - a) * t);
}

float valueNoise(std::uint32_t seed, float x, float y) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float tx = smoothStep(x - static_cast<float>(x0));
    const float ty = smoothStep(y - static_cast<float>(y0));

    const float top = lerp(hashValue(seed, x0, y0), hashValue(seed, x1, y0), tx);
    const float bottom = lerp(hashValue(seed, x0, y1), hashValue(seed, x1, y1), tx);
    return lerp(top, bottom, ty);
}

float fractalNoise(std::uint32_t seed, float x, float y, int octaves) {
    float value = 0.f;
    float amplitude = 0.5f;
    float frequency = 1.f;
    float amplitudeSum = 0.f;

    for (int octave = 0; octave < octaves; ++octave) {
        value += valueNoise(mixSeed(seed, static_cast<std::uint32_t>(octave + 1)), x * frequency, y * frequency) * amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.f;
    }

    if (amplitudeSum <= 0.f) {
        return 0.f;
    }

    return value / amplitudeSum;
}

float computeRadialDistance(int x, int y, int radius) {
    const float dx = static_cast<float>(x - radius) / static_cast<float>(std::max(1, radius));
    const float dy = static_cast<float>(y - radius) / static_cast<float>(std::max(1, radius));
    return std::sqrt((dx * dx) + (dy * dy));
}

bool isProtectedTerrainColumn(int x, int spawnLeftMax, int spawnRightMin, int clearance) {
    return x <= (spawnLeftMax + clearance) || x >= (spawnRightMin - clearance);
}

float selectThreshold(const std::vector<float>& scores, int targetCells, float minimumThreshold) {
    if (scores.empty() || targetCells <= 0) {
        return std::numeric_limits<float>::infinity();
    }

    const int clampedTarget = std::min(targetCells, static_cast<int>(scores.size()));
    std::vector<float> ordered = scores;
    std::nth_element(ordered.begin(), ordered.begin() + (clampedTarget - 1), ordered.end(), std::greater<float>());
    return std::max(minimumThreshold, ordered[clampedTarget - 1]);
}

void pruneSparseMask(std::vector<bool>& mask, const Board& board, int minNeighbours, int passes) {
    const int diameter = board.getDiameter();
    for (int pass = 0; pass < passes; ++pass) {
        std::vector<bool> next = mask;
        for (int y = 0; y < diameter; ++y) {
            for (int x = 0; x < diameter; ++x) {
                const int index = toIndex(x, y, diameter);
                if (!mask[index]) {
                    continue;
                }

                int neighbours = 0;
                for (int direction = 0; direction < 8; ++direction) {
                    const int nx = x + kNeighbourDx[direction];
                    const int ny = y + kNeighbourDy[direction];
                    if (board.isInBounds(nx, ny) && mask[toIndex(nx, ny, diameter)]) {
                        ++neighbours;
                    }
                }

                if (neighbours < minNeighbours) {
                    next[index] = false;
                }
            }
        }
        mask.swap(next);
    }
}

std::vector<TerrainComponent> extractComponents(const Board& board,
                                                const std::vector<bool>& mask,
                                                const std::vector<float>& scores) {
    const int diameter = board.getDiameter();
    std::vector<bool> visited(mask.size(), false);
    std::vector<TerrainComponent> components;

    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const int startIndex = toIndex(x, y, diameter);
            if (!mask[startIndex] || visited[startIndex]) {
                continue;
            }

            TerrainComponent component;
            std::queue<sf::Vector2i> frontier;
            frontier.push({x, y});
            visited[startIndex] = true;
            float scoreSum = 0.f;

            while (!frontier.empty()) {
                const sf::Vector2i current = frontier.front();
                frontier.pop();

                const int currentIndex = toIndex(current.x, current.y, diameter);
                component.cells.push_back(current);
                scoreSum += scores[currentIndex];

                for (int direction = 0; direction < 8; ++direction) {
                    const int nx = current.x + kNeighbourDx[direction];
                    const int ny = current.y + kNeighbourDy[direction];
                    if (!board.isInBounds(nx, ny)) {
                        continue;
                    }

                    const int nextIndex = toIndex(nx, ny, diameter);
                    if (!mask[nextIndex] || visited[nextIndex]) {
                        continue;
                    }

                    visited[nextIndex] = true;
                    frontier.push({nx, ny});
                }
            }

            component.averageScore = scoreSum / static_cast<float>(component.cells.size());
            components.push_back(std::move(component));
        }
    }

    return components;
}

std::vector<sf::Vector2i> limitCellsByScore(const std::vector<sf::Vector2i>& cells,
                                            const std::vector<float>& scores,
                                            int diameter,
                                            int maxCells) {
    if (static_cast<int>(cells.size()) <= maxCells) {
        return cells;
    }

    std::vector<sf::Vector2i> limited = cells;
    std::sort(limited.begin(), limited.end(), [&](const sf::Vector2i& lhs, const sf::Vector2i& rhs) {
        return scores[toIndex(lhs.x, lhs.y, diameter)] > scores[toIndex(rhs.x, rhs.y, diameter)];
    });
    limited.resize(maxCells);
    return limited;
}

std::vector<TerrainComponent> selectComponents(const std::vector<TerrainComponent>& input,
                                               const std::vector<float>& scores,
                                               int diameter,
                                               int maxRegions,
                                               int minCells,
                                               int maxCells,
                                               int targetCells) {
    if (input.empty() || maxRegions <= 0 || targetCells <= 0) {
        return {};
    }

    std::vector<TerrainComponent> ordered = input;
    std::sort(ordered.begin(), ordered.end(), [](const TerrainComponent& lhs, const TerrainComponent& rhs) {
        if (lhs.averageScore != rhs.averageScore) {
            return lhs.averageScore > rhs.averageScore;
        }
        return lhs.cells.size() > rhs.cells.size();
    });

    std::vector<TerrainComponent> selected;
    int selectedCells = 0;

    for (const auto& component : ordered) {
        if (static_cast<int>(selected.size()) >= maxRegions) {
            break;
        }

        TerrainComponent clipped;
        clipped.cells = limitCellsByScore(component.cells, scores, diameter, maxCells);
        if (static_cast<int>(clipped.cells.size()) < minCells) {
            continue;
        }

        float scoreSum = 0.f;
        for (const auto& cell : clipped.cells) {
            scoreSum += scores[toIndex(cell.x, cell.y, diameter)];
        }
        clipped.averageScore = scoreSum / static_cast<float>(clipped.cells.size());

        selectedCells += static_cast<int>(clipped.cells.size());
        selected.push_back(std::move(clipped));

        if (selectedCells >= targetCells) {
            break;
        }
    }

    if (!selected.empty()) {
        return selected;
    }

    TerrainComponent fallback;
    fallback.cells = limitCellsByScore(ordered.front().cells, scores, diameter, maxCells);
    if (static_cast<int>(fallback.cells.size()) >= minCells) {
        float scoreSum = 0.f;
        for (const auto& cell : fallback.cells) {
            scoreSum += scores[toIndex(cell.x, cell.y, diameter)];
        }
        fallback.averageScore = scoreSum / static_cast<float>(fallback.cells.size());
        selected.push_back(std::move(fallback));
    }

    return selected;
}

void applyComponents(Board& board,
                     const std::vector<TerrainComponent>& components,
                     CellType type) {
    for (const auto& component : components) {
        for (const auto& cellPos : component.cells) {
            board.getCell(cellPos.x, cellPos.y).type = type;
        }
    }
}

sf::Vector2i findNearestLandCell(const Board& board, sf::Vector2i target) {
    if (board.isInBounds(target.x, target.y)) {
        const Cell& targetCell = board.getCell(target.x, target.y);
        if (targetCell.isInCircle && targetCell.type != CellType::Water) {
            return target;
        }
    }

    const int diameter = board.getDiameter();
    for (int searchRadius = 1; searchRadius < diameter; ++searchRadius) {
        for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
            for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
                const int nx = target.x + dx;
                const int ny = target.y + dy;
                if (!board.isInBounds(nx, ny)) {
                    continue;
                }

                const Cell& cell = board.getCell(nx, ny);
                if (cell.isInCircle && cell.type != CellType::Water) {
                    return {nx, ny};
                }
            }
        }
    }

    return {board.getRadius(), board.getRadius()};
}

std::vector<std::pair<sf::Vector2i, CellType>> setRegionType(Board& board,
                                                             const std::vector<sf::Vector2i>& cells,
                                                             CellType newType) {
    std::vector<std::pair<sf::Vector2i, CellType>> modified;
    modified.reserve(cells.size());

    for (const auto& cellPos : cells) {
        Cell& cell = board.getCell(cellPos.x, cellPos.y);
        modified.push_back({cellPos, cell.type});
        cell.type = newType;
    }

    return modified;
}

void restoreRegionType(Board& board,
                       const std::vector<std::pair<sf::Vector2i, CellType>>& modified) {
    for (const auto& entry : modified) {
        board.getCell(entry.first.x, entry.first.y).type = entry.second;
    }
}

} // namespace

GenerationResult BoardGenerator::generate(Board& board, const GameConfig& config,
                                          std::vector<Building>& publicBuildings,
                                          std::uint32_t worldSeed) {
    publicBuildings.clear();

    const int radius = board.getRadius();
    const int diameter = board.getDiameter();
    GenerationResult result{{radius / 2, radius}, {radius + (radius / 2), radius}};
    if (diameter <= 0) {
        return result;
    }

    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            Cell& cell = board.getCell(x, y);
            cell.building = nullptr;
            cell.piece = nullptr;
            cell.type = cell.isInCircle ? CellType::Grass : CellType::Void;
        }
    }

    std::mt19937 random(worldSeed == 0 ? 1u : worldSeed);

    const int playerZoneWidth = std::max(2, (diameter * clampInt(config.getPlayerSpawnZonePercent(), 10, 45)) / 100);
    const int aiZoneWidth = std::max(2, (diameter * clampInt(config.getAISpawnZonePercent(), 10, 45)) / 100);
    const int spawnLeftMax = clampInt(playerZoneWidth, 1, diameter - 3);
    const int spawnRightMin = clampInt(diameter - aiZoneWidth - 1, spawnLeftMax + 3, diameter - 2);
    const int terrainClearance = std::max(2, radius / 10);

    const int noiseScale = std::max(5, config.getTerrainNoiseScale());
    const int octaves = std::max(1, config.getTerrainOctaves());

    const auto dirtNoiseSeed = random();
    const auto waterNoiseSeed = random();

    std::vector<float> dirtScores(diameter * diameter, -1.f);
    std::vector<float> waterScores(diameter * diameter, -1.f);
    std::vector<float> dirtCandidateScores;
    std::vector<float> waterCandidateScores;
    int traversableCells = 0;

    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle) {
                continue;
            }

            ++traversableCells;

            const int index = toIndex(x, y, diameter);
            const float localX = static_cast<float>(x - radius) / static_cast<float>(noiseScale);
            const float localY = static_cast<float>(y - radius) / static_cast<float>(noiseScale);
            const float radialDistance = computeRadialDistance(x, y, radius);
            const float edgePenalty = std::max(0.f, radialDistance - 0.82f) * 0.35f;
            const bool protectedColumn = isProtectedTerrainColumn(x, spawnLeftMax, spawnRightMin, terrainClearance);

            const float dirtMacro = fractalNoise(dirtNoiseSeed, localX + 11.7f, localY - 4.9f, octaves);
            const float dirtDetail = fractalNoise(dirtNoiseSeed ^ 0x9e3779b9u, (localX * 2.15f) - 6.2f, (localY * 2.15f) + 9.1f, std::max(1, octaves - 1));
            dirtScores[index] = (dirtMacro * 0.72f) + (dirtDetail * 0.28f) - edgePenalty;

            const float waterMacro = fractalNoise(waterNoiseSeed, (localX * 1.12f) + 3.4f, (localY * 1.12f) - 7.8f, octaves);
            const float waterDetail = fractalNoise(waterNoiseSeed ^ 0x85ebca6bu, (localX * 2.55f) + 5.7f, (localY * 2.55f) - 1.9f, std::max(1, octaves - 1));
            const float waterRingBias = std::max(0.f, 1.f - std::abs(radialDistance - 0.58f));
            waterScores[index] = (waterMacro * 0.67f) + (waterDetail * 0.33f) + (waterRingBias * 0.08f);

            if (!protectedColumn) {
                dirtCandidateScores.push_back(dirtScores[index]);
                waterCandidateScores.push_back(waterScores[index]);
            }
        }
    }

    const int targetDirtCells = (traversableCells * clampInt(config.getDirtCoveragePercent(), 0, 40)) / 100;
    const int targetWaterCells = (traversableCells * clampInt(config.getWaterCoveragePercent(), 0, 12)) / 100;
    const float dirtThreshold = selectThreshold(dirtCandidateScores, targetDirtCells, 0.57f);
    const float waterThreshold = selectThreshold(waterCandidateScores, targetWaterCells, 0.69f);

    std::vector<bool> dirtMask(diameter * diameter, false);
    std::vector<bool> waterMask(diameter * diameter, false);
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle || isProtectedTerrainColumn(x, spawnLeftMax, spawnRightMin, terrainClearance)) {
                continue;
            }

            const int index = toIndex(x, y, diameter);
            dirtMask[index] = dirtScores[index] >= dirtThreshold;
            waterMask[index] = waterScores[index] >= waterThreshold;
        }
    }

    pruneSparseMask(dirtMask, board, 2, 2);
    pruneSparseMask(waterMask, board, 3, 2);

    const auto dirtComponents = extractComponents(board, dirtMask, dirtScores);
    const auto waterComponents = extractComponents(board, waterMask, waterScores);

    const int desiredDirtPerRegion = std::max(1, targetDirtCells / std::max(1, config.getNumDirtBlobs()));
    const int desiredWaterPerRegion = std::max(1, targetWaterCells / std::max(1, config.getNumLakes()));
    const int minDirtCells = std::max(4, std::min(regionCellCountFromRadius(config.getDirtBlobMinRadius()) / 2, desiredDirtPerRegion));
    const int maxDirtCells = std::max(regionCellCountFromRadius(config.getDirtBlobMaxRadius()), desiredDirtPerRegion + (desiredDirtPerRegion / 2));
    const int minWaterCells = std::max(3, std::min(regionCellCountFromRadius(config.getLakeMinRadius()) / 2, desiredWaterPerRegion));
    const int maxWaterCells = std::max(regionCellCountFromRadius(config.getLakeMaxRadius()), desiredWaterPerRegion + (desiredWaterPerRegion / 2));

    const auto selectedDirt = selectComponents(
        dirtComponents,
        dirtScores,
        diameter,
        std::max(1, config.getNumDirtBlobs()),
        minDirtCells,
        maxDirtCells,
        targetDirtCells);
    const auto selectedWater = selectComponents(
        waterComponents,
        waterScores,
        diameter,
        std::max(1, config.getNumLakes()),
        minWaterCells,
        maxWaterCells,
        targetWaterCells);

    applyComponents(board, selectedDirt, CellType::Dirt);

    for (const auto& waterRegion : selectedWater) {
        const auto modified = setRegionType(board, waterRegion.cells, CellType::Water);
        const sf::Vector2i leftTarget = findNearestLandCell(board, {std::max(1, spawnLeftMax / 2), radius});
        const sf::Vector2i rightTarget = findNearestLandCell(board, {std::min(diameter - 2, spawnRightMin + ((diameter - 1 - spawnRightMin) / 2)), radius});
        if (!isConnected(board, leftTarget, rightTarget)) {
            restoreRegionType(board, modified);
        }
    }

    applyTerrainVisuals(board, worldSeed);

    std::uniform_int_distribution<int> rotationDist(0, 3);
    std::uniform_int_distribution<int> flipMaskDist(0, kFlipHorizontalMask | kFlipVerticalMask);

    const int churchW = config.getBuildingWidth(BuildingType::Church);
    const int churchH = config.getBuildingHeight(BuildingType::Church);
    const int churchRotation = rotationDist(random);
    const int churchFlipMask = flipMaskDist(random);
    const int churchFootprintW = Building::getFootprintWidthFor(churchW, churchH, churchRotation);
    const int churchFootprintH = Building::getFootprintHeightFor(churchW, churchH, churchRotation);
    const sf::Vector2i churchPos = findValidBuildingPos(board, publicBuildings, churchFootprintW, churchFootprintH,
                                                        0, spawnLeftMax, spawnRightMin, random);
    {
        Building church;
        church.id = 0;
        church.type = BuildingType::Church;
        church.owner = KingdomId::White;
        church.origin = churchPos;
        church.width = churchW;
        church.height = churchH;
        church.rotationQuarterTurns = churchRotation;
        church.flipMask = churchFlipMask;
        church.isNeutral = true;
        church.cellHP.assign(churchW * churchH, 999);
        church.isProducing = false;
        church.turnsRemaining = 0;
        publicBuildings.push_back(church);

        for (const auto& occupiedCell : publicBuildings.back().getOccupiedCells()) {
            board.getCell(occupiedCell.x, occupiedCell.y).building = &publicBuildings.back();
        }
    }

    const int minDist = config.getMinPublicBuildingDistance();

    for (int i = 0; i < config.getNumMines(); ++i) {
        const int mineW = config.getBuildingWidth(BuildingType::Mine);
        const int mineH = config.getBuildingHeight(BuildingType::Mine);
        const int mineRotation = rotationDist(random);
        const int mineFlipMask = flipMaskDist(random);
        const int mineFootprintW = Building::getFootprintWidthFor(mineW, mineH, mineRotation);
        const int mineFootprintH = Building::getFootprintHeightFor(mineW, mineH, mineRotation);
        const sf::Vector2i pos = findValidBuildingPos(board, publicBuildings, mineFootprintW, mineFootprintH,
                                                      minDist, spawnLeftMax, spawnRightMin, random);
        Building mine;
        mine.id = static_cast<int>(publicBuildings.size());
        mine.type = BuildingType::Mine;
        mine.owner = KingdomId::White;
        mine.origin = pos;
        mine.width = mineW;
        mine.height = mineH;
        mine.rotationQuarterTurns = mineRotation;
        mine.flipMask = mineFlipMask;
        mine.isNeutral = true;
        mine.cellHP.assign(mineW * mineH, 999);
        mine.isProducing = false;
        mine.turnsRemaining = 0;
        publicBuildings.push_back(mine);

        for (const auto& occupiedCell : publicBuildings.back().getOccupiedCells()) {
            board.getCell(occupiedCell.x, occupiedCell.y).building = &publicBuildings.back();
        }
    }

    for (int i = 0; i < config.getNumFarms(); ++i) {
        const int farmW = config.getBuildingWidth(BuildingType::Farm);
        const int farmH = config.getBuildingHeight(BuildingType::Farm);
        const int farmRotation = rotationDist(random);
        const int farmFlipMask = flipMaskDist(random);
        const int farmFootprintW = Building::getFootprintWidthFor(farmW, farmH, farmRotation);
        const int farmFootprintH = Building::getFootprintHeightFor(farmW, farmH, farmRotation);
        const sf::Vector2i pos = findValidBuildingPos(board, publicBuildings, farmFootprintW, farmFootprintH,
                                                      minDist, spawnLeftMax, spawnRightMin, random);
        Building farm;
        farm.id = static_cast<int>(publicBuildings.size());
        farm.type = BuildingType::Farm;
        farm.owner = KingdomId::White;
        farm.origin = pos;
        farm.width = farmW;
        farm.height = farmH;
        farm.rotationQuarterTurns = farmRotation;
        farm.flipMask = farmFlipMask;
        farm.isNeutral = true;
        farm.cellHP.assign(farmW * farmH, 999);
        farm.isProducing = false;
        farm.turnsRemaining = 0;
        publicBuildings.push_back(farm);

        for (const auto& occupiedCell : publicBuildings.back().getOccupiedCells()) {
            board.getCell(occupiedCell.x, occupiedCell.y).building = &publicBuildings.back();
        }
    }

    result.playerSpawn = findSpawnCell(board, 1, spawnLeftMax, random);
    result.aiSpawn = findSpawnCell(board, spawnRightMin, diameter - 2, random);
    return result;
}

void BoardGenerator::applyTerrainVisuals(Board& board, std::uint32_t worldSeed) {
    const int diameter = board.getDiameter();
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle || cell.type == CellType::Void) {
                cell.terrainFlipMask = 0;
                continue;
            }

            cell.terrainFlipMask = terrainFlipMaskFor(worldSeed, x, y, cell.type);
        }
    }
}

bool BoardGenerator::isConnected(const Board& board, sf::Vector2i from, sf::Vector2i to) {
    if (from == to) return true;
    const int diameter = board.getDiameter();

    std::queue<sf::Vector2i> queue;
    std::vector<std::vector<bool>> visited(diameter, std::vector<bool>(diameter, false));

    queue.push(from);
    visited[from.y][from.x] = true;

    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};

    int steps = 0;
    const int maxSteps = diameter * diameter;

    while (!queue.empty() && steps < maxSteps) {
        const sf::Vector2i current = queue.front();
        queue.pop();
        ++steps;

        if (current == to) return true;

        for (int direction = 0; direction < 4; ++direction) {
            const int nx = current.x + dx[direction];
            const int ny = current.y + dy[direction];
            if (nx < 0 || nx >= diameter || ny < 0 || ny >= diameter || visited[ny][nx]) {
                continue;
            }

            const Cell& cell = board.getCell(nx, ny);
            if (!cell.isInCircle || cell.type == CellType::Water) {
                continue;
            }

            visited[ny][nx] = true;
            queue.push({nx, ny});
        }
    }
    return false;
}

sf::Vector2i BoardGenerator::findValidBuildingPos(const Board& board,
                                                  const std::vector<Building>& existing,
                                                  int width, int height, int minDist,
                                                  int avoidLeftX, int avoidRightX,
                                                  std::mt19937& random) {
    const int diameter = board.getDiameter();
    const int radius = board.getRadius();
    std::vector<sf::Vector2i> candidates;

    for (int y = 1; y + height < diameter; ++y) {
        for (int x = std::max(1, avoidLeftX); x + width <= std::min(avoidRightX, diameter - 1); ++x) {
            if (!canPlaceBuilding(board, {x, y}, width, height)) {
                continue;
            }

            bool tooClose = false;
            for (const auto& building : existing) {
                const float bx = static_cast<float>(building.origin.x + (building.getFootprintWidth() / 2));
                const float by = static_cast<float>(building.origin.y + (building.getFootprintHeight() / 2));
                const float px = static_cast<float>(x + (width / 2));
                const float py = static_cast<float>(y + (height / 2));
                const float distance = std::sqrt(((bx - px) * (bx - px)) + ((by - py) * (by - py)));
                if (distance < static_cast<float>(minDist)) {
                    tooClose = true;
                    break;
                }
            }

            if (!tooClose) {
                candidates.push_back({x, y});
            }
        }
    }

    if (!candidates.empty()) {
        std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
        return candidates[dist(random)];
    }

    sf::Vector2i fallback{radius - (width / 2), radius - (height / 2)};
    if (canPlaceBuilding(board, fallback, width, height)) {
        return fallback;
    }

    for (int y = 1; y + height < diameter; ++y) {
        for (int x = 1; x + width < diameter; ++x) {
            if (canPlaceBuilding(board, {x, y}, width, height)) {
                return {x, y};
            }
        }
    }

    return fallback;
}

sf::Vector2i BoardGenerator::findSpawnCell(const Board& board,
                                           int minX, int maxX,
                                           std::mt19937& random) {
    const int diameter = board.getDiameter();
    std::vector<sf::Vector2i> candidates;

    for (int y = 1; y < diameter - 1; ++y) {
        for (int x = std::max(1, minX); x <= std::min(maxX, diameter - 2); ++x) {
            const Cell& cell = board.getCell(x, y);
            if (cell.isInCircle && cell.type != CellType::Water && !cell.building) {
                candidates.push_back({x, y});
            }
        }
    }

    if (!candidates.empty()) {
        std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
        return candidates[dist(random)];
    }

    for (int y = 1; y < diameter - 1; ++y) {
        for (int x = 1; x < diameter - 1; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (cell.isInCircle && cell.type != CellType::Water && !cell.building) {
                return {x, y};
            }
        }
    }

    return {board.getRadius(), board.getRadius()};
}

bool BoardGenerator::canPlaceBuilding(const Board& board, sf::Vector2i origin, int w, int h) {
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            const int x = origin.x + dx;
            const int y = origin.y + dy;
            if (!board.isInBounds(x, y)) return false;
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle) return false;
            if (cell.type == CellType::Water) return false;
            if (cell.building) return false;
        }
    }
    return true;
}
