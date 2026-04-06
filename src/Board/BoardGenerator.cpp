#include "Board/BoardGenerator.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Board/CellType.hpp"
#include "Config/GameConfig.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include <cmath>
#include <random>
#include <queue>
#include <set>
#include <algorithm>

static std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

GenerationResult BoardGenerator::generate(Board& board, const GameConfig& config,
                                           std::vector<Building>& publicBuildings) {
    int radius = config.getMapRadius();
    board.init(radius);
    int diameter = board.getDiameter();

    placeDirtBlobs(board, config);
    placeWaterLakes(board, config);

    // Spawn zones: left 25% and right 25%
    int spawnLeftMax = static_cast<int>(radius * 0.5f);
    int spawnRightMin = static_cast<int>(radius * 1.5f);

    // Place church (1) near center
    int churchW = config.getBuildingWidth(BuildingType::Church);
    int churchH = config.getBuildingHeight(BuildingType::Church);
    sf::Vector2i churchPos = findValidBuildingPos(board, publicBuildings, churchW, churchH,
                                                   0, spawnLeftMax, spawnRightMin);
    {
        Building church;
        church.id = 0;
        church.type = BuildingType::Church;
        church.owner = KingdomId::White; // Neutral, but needs a value
        church.origin = churchPos;
        church.width = churchW;
        church.height = churchH;
        church.isNeutral = true;
        church.cellHP.assign(churchW * churchH, 999);
        church.isProducing = false;
        church.turnsRemaining = 0;
        publicBuildings.push_back(church);

        for (int dy = 0; dy < churchH; ++dy) {
            for (int dx = 0; dx < churchW; ++dx) {
                Cell& c = board.getCell(churchPos.x + dx, churchPos.y + dy);
                c.building = &publicBuildings.back();
            }
        }
    }

    int minDist = config.getMinPublicBuildingDistance();

    // Place mines (2)
    for (int i = 0; i < config.getNumMines(); ++i) {
        int mineW = config.getBuildingWidth(BuildingType::Mine);
        int mineH = config.getBuildingHeight(BuildingType::Mine);
        sf::Vector2i pos = findValidBuildingPos(board, publicBuildings, mineW, mineH,
                                                 minDist, spawnLeftMax, spawnRightMin);
        Building mine;
        mine.id = static_cast<int>(publicBuildings.size());
        mine.type = BuildingType::Mine;
        mine.owner = KingdomId::White;
        mine.origin = pos;
        mine.width = mineW;
        mine.height = mineH;
        mine.isNeutral = true;
        mine.cellHP.assign(mineW * mineH, 999);
        mine.isProducing = false;
        mine.turnsRemaining = 0;
        publicBuildings.push_back(mine);

        for (int dy = 0; dy < mineH; ++dy) {
            for (int dx = 0; dx < mineW; ++dx) {
                Cell& c = board.getCell(pos.x + dx, pos.y + dy);
                c.building = &publicBuildings.back();
            }
        }
    }

    // Place farms (3)
    for (int i = 0; i < config.getNumFarms(); ++i) {
        int farmW = config.getBuildingWidth(BuildingType::Farm);
        int farmH = config.getBuildingHeight(BuildingType::Farm);
        sf::Vector2i pos = findValidBuildingPos(board, publicBuildings, farmW, farmH,
                                                 minDist, spawnLeftMax, spawnRightMin);
        Building farm;
        farm.id = static_cast<int>(publicBuildings.size());
        farm.type = BuildingType::Farm;
        farm.owner = KingdomId::White;
        farm.origin = pos;
        farm.width = farmW;
        farm.height = farmH;
        farm.isNeutral = true;
        farm.cellHP.assign(farmW * farmH, 999);
        farm.isProducing = false;
        farm.turnsRemaining = 0;
        publicBuildings.push_back(farm);

        for (int dy = 0; dy < farmH; ++dy) {
            for (int dx = 0; dx < farmW; ++dx) {
                Cell& c = board.getCell(pos.x + dx, pos.y + dy);
                c.building = &publicBuildings.back();
            }
        }
    }

    // Place initial pawns
    GenerationResult result;

    // Player spawn: left 25%
    {
        std::uniform_int_distribution<int> distX(1, spawnLeftMax);
        std::uniform_int_distribution<int> distY(1, diameter - 2);
        for (int attempt = 0; attempt < 1000; ++attempt) {
            int x = distX(rng());
            int y = distY(rng());
            const Cell& c = board.getCell(x, y);
            if (c.isInCircle && c.type != CellType::Water && !c.building) {
                result.playerSpawn = {x, y};
                break;
            }
        }
    }

    // AI spawn: right 25%
    {
        std::uniform_int_distribution<int> distX(spawnRightMin, diameter - 2);
        std::uniform_int_distribution<int> distY(1, diameter - 2);
        for (int attempt = 0; attempt < 1000; ++attempt) {
            int x = distX(rng());
            int y = distY(rng());
            const Cell& c = board.getCell(x, y);
            if (c.isInCircle && c.type != CellType::Water && !c.building) {
                result.aiSpawn = {x, y};
                break;
            }
        }
    }

    return result;
}

void BoardGenerator::placeDirtBlobs(Board& board, const GameConfig& config) {
    int diameter = board.getDiameter();
    int radius = board.getRadius();
    int numBlobs = config.getNumDirtBlobs();
    int minR = config.getDirtBlobMinRadius();
    int maxR = config.getDirtBlobMaxRadius();

    std::uniform_int_distribution<int> posDist(0, diameter - 1);
    std::uniform_int_distribution<int> radDist(minR, maxR);

    for (int i = 0; i < numBlobs; ++i) {
        int cx = posDist(rng());
        int cy = posDist(rng());
        int r = radDist(rng());

        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy <= r * r) {
                    int nx = cx + dx;
                    int ny = cy + dy;
                    if (board.isInBounds(nx, ny)) {
                        Cell& c = board.getCell(nx, ny);
                        if (c.isInCircle && c.type == CellType::Grass) {
                            c.type = CellType::Dirt;
                        }
                    }
                }
            }
        }
    }
}

void BoardGenerator::placeWaterLakes(Board& board, const GameConfig& config) {
    int diameter = board.getDiameter();
    int radius = board.getRadius();
    int spawnLeftMax = static_cast<int>(radius * 0.5f);
    int spawnRightMin = static_cast<int>(radius * 1.5f);
    int numLakes = config.getNumLakes();
    int minR = config.getLakeMinRadius();
    int maxR = config.getLakeMaxRadius();

    std::uniform_int_distribution<int> posDist(maxR + 1, diameter - maxR - 2);
    std::uniform_int_distribution<int> radDist(minR, maxR);

    for (int i = 0; i < numLakes; ++i) {
        int cx = posDist(rng());
        int cy = posDist(rng());

        // Don't place in spawn zones
        if (cx < spawnLeftMax || cx > spawnRightMin) continue;

        int r = radDist(rng());

        // Save state in case we need to revert
        std::vector<std::pair<sf::Vector2i, CellType>> modified;

        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy <= r * r) {
                    int nx = cx + dx;
                    int ny = cy + dy;
                    if (board.isInBounds(nx, ny)) {
                        Cell& c = board.getCell(nx, ny);
                        if (c.isInCircle && (c.type == CellType::Grass || c.type == CellType::Dirt)) {
                            modified.push_back({{nx, ny}, c.type});
                            c.type = CellType::Water;
                        }
                    }
                }
            }
        }

        // Connectivity check between left and right sides
        sf::Vector2i left = {spawnLeftMax / 2, radius};
        sf::Vector2i right = {spawnRightMin + (diameter - spawnRightMin) / 2, radius};

        // Find nearest valid cell to these targets
        auto findNearest = [&](sf::Vector2i target) -> sf::Vector2i {
            for (int searchR = 0; searchR < 50; ++searchR) {
                for (int dy2 = -searchR; dy2 <= searchR; ++dy2) {
                    for (int dx2 = -searchR; dx2 <= searchR; ++dx2) {
                        int nx2 = target.x + dx2;
                        int ny2 = target.y + dy2;
                        if (board.isInBounds(nx2, ny2)) {
                            const Cell& c2 = board.getCell(nx2, ny2);
                            if (c2.isInCircle && c2.type != CellType::Water) {
                                return {nx2, ny2};
                            }
                        }
                    }
                }
            }
            return target;
        };

        left = findNearest(left);
        right = findNearest(right);

        if (!isConnected(board, left, right)) {
            // Revert
            for (auto& [pos, oldType] : modified) {
                board.getCell(pos.x, pos.y).type = oldType;
            }
        }
    }
}

bool BoardGenerator::isConnected(const Board& board, sf::Vector2i from, sf::Vector2i to) {
    if (from == to) return true;
    int diameter = board.getDiameter();

    // BFS with limited steps to avoid huge search on 1024x1024
    std::queue<sf::Vector2i> queue;
    std::vector<std::vector<bool>> visited(diameter, std::vector<bool>(diameter, false));

    queue.push(from);
    visited[from.y][from.x] = true;

    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};

    int steps = 0;
    int maxSteps = diameter * diameter / 4; // limit search

    while (!queue.empty() && steps < maxSteps) {
        sf::Vector2i cur = queue.front();
        queue.pop();
        ++steps;

        if (cur == to) return true;

        for (int d = 0; d < 4; ++d) {
            int nx = cur.x + dx[d];
            int ny = cur.y + dy[d];
            if (nx >= 0 && nx < diameter && ny >= 0 && ny < diameter && !visited[ny][nx]) {
                const Cell& c = board.getCell(nx, ny);
                if (c.isInCircle && c.type != CellType::Water) {
                    visited[ny][nx] = true;
                    queue.push({nx, ny});
                }
            }
        }
    }
    return false;
}

sf::Vector2i BoardGenerator::findValidBuildingPos(const Board& board,
                                                    const std::vector<Building>& existing,
                                                    int width, int height, int minDist,
                                                    int avoidLeftX, int avoidRightX) {
    int diameter = board.getDiameter();
    int radius = board.getRadius();
    std::uniform_int_distribution<int> distX(radius / 2, radius + radius / 2);
    std::uniform_int_distribution<int> distY(radius / 2, radius + radius / 2);

    for (int attempt = 0; attempt < 5000; ++attempt) {
        int x = distX(rng());
        int y = distY(rng());

        // Check not in spawn zones
        if (x < avoidLeftX || x + width > avoidRightX) {
            // Allow center area only
            if (x < avoidLeftX) continue;
        }

        if (!canPlaceBuilding(board, {x, y}, width, height)) continue;

        // Check min distance from existing
        bool tooClose = false;
        for (const auto& b : existing) {
            float bx = static_cast<float>(b.origin.x + b.width / 2);
            float by = static_cast<float>(b.origin.y + b.height / 2);
            float px = static_cast<float>(x + width / 2);
            float py = static_cast<float>(y + height / 2);
            float dist = std::sqrt((bx - px) * (bx - px) + (by - py) * (by - py));
            if (dist < static_cast<float>(minDist)) {
                tooClose = true;
                break;
            }
        }
        if (tooClose) continue;

        return {x, y};
    }

    // Fallback: place near center
    return {radius - width / 2, radius - height / 2};
}

bool BoardGenerator::canPlaceBuilding(const Board& board, sf::Vector2i origin, int w, int h) {
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            int x = origin.x + dx;
            int y = origin.y + dy;
            if (!board.isInBounds(x, y)) return false;
            const Cell& c = board.getCell(x, y);
            if (!c.isInCircle) return false;
            if (c.type == CellType::Water) return false;
            if (c.building) return false;
        }
    }
    return true;
}
