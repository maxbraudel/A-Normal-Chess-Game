#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <SFML/System/Vector2.hpp>

#include "Board/Board.hpp"
#include "Board/BoardGenerator.hpp"
#include "Board/CellType.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Objects/MapObject.hpp"
#include "Systems/ChestLootProgression.hpp"
#include "Systems/ChestSystem.hpp"
#include "Systems/WeatherSystem.hpp"
#include "Systems/WeatherTypes.hpp"
#include "Systems/XPSystem.hpp"
#include "Systems/XPTypes.hpp"

namespace {

constexpr int kBrightnessBinCount = 16;
constexpr int kExampleSampleCount = 5;

struct BatchOptions {
    std::filesystem::path outputPath =
        std::filesystem::path("debug_game_state") / "statistics" / "randomness_statistics.json";
    std::uint32_t baseSeed = 1337u;
    int sampleCount = 500;
    int turnBudget = 100;
};

struct MetricSamples {
    std::vector<double> values;

    void add(double value) {
        values.push_back(value);
    }
};

struct BoardExample {
    std::uint32_t seed = 0;
    int validCells = 0;
    int grassCells = 0;
    int dirtCells = 0;
    int waterCells = 0;
    int waterComponentCount = 0;
    int largestWaterComponent = 0;
    double playerAiSpawnDistance = 0.0;
    double mineToLakeDistance = 0.0;
    double farmToLakeDistance = 0.0;
    double mineToFarmDistance = 0.0;
    sf::Vector2i playerSpawn{0, 0};
    sf::Vector2i aiSpawn{0, 0};
};

struct WaterComponent {
    std::vector<sf::Vector2i> cells;
};

struct MapSuiteResults {
    MetricSamples validCells;
    MetricSamples grassCells;
    MetricSamples dirtCells;
    MetricSamples waterCells;
    MetricSamples dirtCoveragePercent;
    MetricSamples waterCoveragePercent;
    MetricSamples waterComponentCount;
    MetricSamples largestWaterComponent;
    MetricSamples playerAiSpawnDistance;
    MetricSamples mineToLakeDistance;
    MetricSamples farmToLakeDistance;
    MetricSamples mineToFarmDistance;

    std::map<std::string, std::uint64_t> playerSpawnHeatmap;
    std::map<std::string, std::uint64_t> aiSpawnHeatmap;
    std::map<std::string, std::uint64_t> mineOriginHeatmap;
    std::map<std::string, std::uint64_t> farmOriginHeatmap;
    std::map<int, std::uint64_t> waterComponentSizeHistogram;

    std::map<std::string, std::array<std::uint64_t, 4>> terrainFlipHistogram;
    std::map<std::string, std::array<std::uint64_t, kBrightnessBinCount>> brightnessHistogram;
    std::map<std::string, std::array<std::uint64_t, 4>> buildingRotationHistogram;
    std::map<std::string, std::array<std::uint64_t, 4>> buildingFlipHistogram;

    std::vector<BoardExample> examples;
};

struct XPSourceSuiteResults {
    XPRewardProfile profile{};
    MetricSamples amounts;
    std::map<int, std::uint64_t> amountHistogram;
};

struct XPSuiteResults {
    std::map<std::string, XPSourceSuiteResults> sources;
};

struct ChestRewardPhaseResults {
    std::uint64_t rewardCount = 0;
    std::map<std::string, std::uint64_t> rewardTypeCounts;
    MetricSamples goldAmounts;
    std::map<int, std::uint64_t> goldAmountHistogram;
};

struct ChestTimelineResults {
    MetricSamples collectedChests;
    MetricSamples totalGold;
    MetricSamples totalMovementBonus;
    MetricSamples totalBuildBonus;
};

struct ChestSuiteResults {
    MetricSamples spawnDelayTurns;
    std::map<int, std::uint64_t> spawnDelayHistogram;
    ChestRewardPhaseResults earlyPhase;
    ChestRewardPhaseResults latePhase;
    ChestTimelineResults immediateCollectionTimeline;
};

struct WeatherExample {
    std::uint32_t seed = 0;
    std::string direction;
    int spawnStep = 0;
    int durationTurnSteps = 0;
    double aspectRatio = 0.0;
    double visibleCoveragePercent = 0.0;
    double meanAlpha = 0.0;
    int maxAlpha = 0;
    int simultaneousFrontsAtSpawn = 0;
};

struct WeatherSuiteResults {
    MetricSamples arrivalDelayTurns;
    std::map<int, std::uint64_t> arrivalDelayHistogram;
    std::map<std::string, std::uint64_t> directionCounts;
    MetricSamples coveragePercent;
    std::map<int, std::uint64_t> coverageHistogram;
    MetricSamples aspectRatio;
    MetricSamples durationTurns;
    std::map<int, std::uint64_t> durationHistogram;
    MetricSamples isolatedVisibleCellCount;
    MetricSamples isolatedVisibleCoveragePercent;
    MetricSamples isolatedMeanAlpha;
    MetricSamples isolatedMaxAlpha;
    MetricSamples spawnIntervalTurns;
    MetricSamples activeFrontCountPerStep;
    MetricSamples maxActiveFrontsPerWorld;
    MetricSamples spawnedFrontCountPerWorld;
    std::map<int, std::uint64_t> activeFrontCountHistogram;
    std::vector<WeatherExample> examples;
};

BatchOptions parseArgs(int argc, char** argv) {
    BatchOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        auto requireValue = [&](const std::string& name) -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error("Missing value for argument: " + name);
            }
            return argv[++index];
        };

        if (argument == "--output") {
            options.outputPath = requireValue(argument);
        } else if (argument == "--samples") {
            options.sampleCount = std::stoi(requireValue(argument));
        } else if (argument == "--turns") {
            options.turnBudget = std::stoi(requireValue(argument));
        } else if (argument == "--base-seed") {
            options.baseSeed = static_cast<std::uint32_t>(std::stoul(requireValue(argument)));
        } else if (argument == "--help" || argument == "-h") {
            std::cout << "Usage: ANormalChessGameBatchStats [--output path] [--samples N] [--turns N] [--base-seed N]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + argument);
        }
    }

    if (options.sampleCount <= 0) {
        throw std::runtime_error("--samples must be strictly positive.");
    }
    if (options.turnBudget <= 0) {
        throw std::runtime_error("--turns must be strictly positive.");
    }

    return options;
}

std::string escapeJsonString(const std::string& value) {
    std::ostringstream escaped;
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped << "\\\\";
            break;
        case '"':
            escaped << "\\\"";
            break;
        case '\n':
            escaped << "\\n";
            break;
        case '\r':
            escaped << "\\r";
            break;
        case '\t':
            escaped << "\\t";
            break;
        default:
            escaped << character;
            break;
        }
    }
    return escaped.str();
}

std::string toJsonKey(const sf::Vector2i& position) {
    return std::to_string(position.x) + "," + std::to_string(position.y);
}

void recordHeatmap(std::map<std::string, std::uint64_t>& heatmap, const sf::Vector2i& position) {
    ++heatmap[toJsonKey(position)];
}

std::string buildingTypeName(BuildingType type) {
    switch (type) {
    case BuildingType::Church:
        return "Church";
    case BuildingType::Mine:
        return "Mine";
    case BuildingType::Farm:
        return "Farm";
    case BuildingType::Barracks:
        return "Barracks";
    case BuildingType::WoodWall:
        return "WoodWall";
    case BuildingType::StoneWall:
        return "StoneWall";
    case BuildingType::Bridge:
        return "Bridge";
    case BuildingType::Arena:
        return "Arena";
    }

    return "Unknown";
}

std::string cellTypeName(CellType type) {
    switch (type) {
    case CellType::Void:
        return "Void";
    case CellType::Grass:
        return "Grass";
    case CellType::Dirt:
        return "Dirt";
    case CellType::Water:
        return "Water";
    }

    return "Unknown";
}

std::string xpRewardSourceName(XPRewardSource source) {
    switch (source) {
    case XPRewardSource::KillPawn:
        return "KillPawn";
    case XPRewardSource::KillKnight:
        return "KillKnight";
    case XPRewardSource::KillBishop:
        return "KillBishop";
    case XPRewardSource::KillRook:
        return "KillRook";
    case XPRewardSource::KillQueen:
        return "KillQueen";
    case XPRewardSource::DestroyBlock:
        return "DestroyBlock";
    case XPRewardSource::ArenaPerTurn:
        return "ArenaPerTurn";
    case XPRewardSource::Count:
        break;
    }

    return "Unknown";
}

std::string weatherDirectionName(WeatherDirection direction) {
    switch (direction) {
    case WeatherDirection::North:
        return "North";
    case WeatherDirection::South:
        return "South";
    case WeatherDirection::East:
        return "East";
    case WeatherDirection::West:
        return "West";
    case WeatherDirection::NorthEast:
        return "NorthEast";
    case WeatherDirection::NorthWest:
        return "NorthWest";
    case WeatherDirection::SouthEast:
        return "SouthEast";
    case WeatherDirection::SouthWest:
        return "SouthWest";
    case WeatherDirection::Count:
        break;
    }

    return "Unknown";
}

double euclideanDistance(const sf::Vector2i& lhs, const sf::Vector2i& rhs) {
    const double dx = static_cast<double>(lhs.x - rhs.x);
    const double dy = static_cast<double>(lhs.y - rhs.y);
    return std::sqrt((dx * dx) + (dy * dy));
}

double minDistance(const std::vector<sf::Vector2i>& lhs, const std::vector<sf::Vector2i>& rhs) {
    if (lhs.empty() || rhs.empty()) {
        return 0.0;
    }

    double best = std::numeric_limits<double>::max();
    for (const sf::Vector2i& leftCell : lhs) {
        for (const sf::Vector2i& rightCell : rhs) {
            best = std::min(best, euclideanDistance(leftCell, rightCell));
        }
    }
    return best;
}

double meanMinimumDistance(const std::vector<std::vector<sf::Vector2i>>& fromSets,
                           const std::vector<std::vector<sf::Vector2i>>& toSets) {
    if (fromSets.empty() || toSets.empty()) {
        return 0.0;
    }

    double total = 0.0;
    for (const std::vector<sf::Vector2i>& fromSet : fromSets) {
        double best = std::numeric_limits<double>::max();
        for (const std::vector<sf::Vector2i>& toSet : toSets) {
            best = std::min(best, minDistance(fromSet, toSet));
        }
        total += best;
    }

    return total / static_cast<double>(fromSets.size());
}

std::vector<WaterComponent> findWaterComponents(const Board& board) {
    const int diameter = board.getDiameter();
    std::vector<WaterComponent> components;
    std::vector<std::vector<bool>> visited(diameter, std::vector<bool>(diameter, false));
    constexpr int kDirections[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            if (visited[y][x]) {
                continue;
            }

            const Cell& originCell = board.getCell(x, y);
            if (!originCell.isInCircle || originCell.type != CellType::Water) {
                continue;
            }

            visited[y][x] = true;
            std::queue<sf::Vector2i> frontier;
            frontier.push({x, y});

            WaterComponent component;
            while (!frontier.empty()) {
                const sf::Vector2i current = frontier.front();
                frontier.pop();
                component.cells.push_back(current);

                for (const auto& direction : kDirections) {
                    const int nx = current.x + direction[0];
                    const int ny = current.y + direction[1];
                    if (nx < 0 || nx >= diameter || ny < 0 || ny >= diameter || visited[ny][nx]) {
                        continue;
                    }

                    const Cell& neighbor = board.getCell(nx, ny);
                    if (!neighbor.isInCircle || neighbor.type != CellType::Water) {
                        continue;
                    }

                    visited[ny][nx] = true;
                    frontier.push({nx, ny});
                }
            }

            components.push_back(std::move(component));
        }
    }

    return components;
}

int brightnessBin(std::uint8_t brightness) {
    const int scaled = (static_cast<int>(brightness) * kBrightnessBinCount) / 256;
    return std::clamp(scaled, 0, kBrightnessBinCount - 1);
}

std::string formatDouble(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
}

double discreteUniformMean(int minValue, int maxValue, double scale = 1.0) {
    return ((static_cast<double>(minValue) + static_cast<double>(maxValue)) * 0.5) * scale;
}

double discreteUniformVariance(int minValue, int maxValue, double scale = 1.0) {
    const double count = static_cast<double>((maxValue - minValue) + 1);
    const double baseVariance = ((count * count) - 1.0) / 12.0;
    return baseVariance * scale * scale;
}

double weibullMean(double shape, double scale) {
    return scale * std::tgamma(1.0 + (1.0 / shape));
}

double weibullVariance(double shape, double scale) {
    const double meanFactor = std::tgamma(1.0 + (1.0 / shape));
    const double secondMomentFactor = std::tgamma(1.0 + (2.0 / shape));
    return (scale * scale) * (secondMomentFactor - (meanFactor * meanFactor));
}

double gammaMean(double shape, double scale, double offset = 0.0) {
    return offset + (shape * scale);
}

double gammaVariance(double shape, double scale) {
    return shape * scale * scale;
}

double xpProfileSigma(const XPRewardProfile& profile) {
    return std::max(1.0, static_cast<double>(profile.mean)
        * (static_cast<double>(profile.sigmaMultiplierTimes100) / 100.0));
}

double xpProfileClampDelta(const XPRewardProfile& profile) {
    return xpProfileSigma(profile)
        * (static_cast<double>(profile.clampSigmaMultiplierTimes100) / 100.0);
}

std::map<std::string, double> probabilityMapFromCounts(const std::map<std::string, std::uint64_t>& counts) {
    std::map<std::string, double> probabilities;
    const std::uint64_t total = std::accumulate(
        counts.begin(),
        counts.end(),
        std::uint64_t{0},
        [](std::uint64_t sum, const auto& entry) { return sum + entry.second; });
    if (total == 0) {
        return probabilities;
    }

    for (const auto& entry : counts) {
        probabilities[entry.first] = static_cast<double>(entry.second) / static_cast<double>(total);
    }
    return probabilities;
}

template <typename KeyType>
std::string toJsonObjectKey(const KeyType& value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

void writeIndent(std::ostream& output, int indentLevel) {
    for (int index = 0; index < indentLevel; ++index) {
        output << "  ";
    }
}

double quantile(std::vector<double> values, double q) {
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    const double position = q * static_cast<double>(values.size() - 1);
    const std::size_t lowerIndex = static_cast<std::size_t>(std::floor(position));
    const std::size_t upperIndex = static_cast<std::size_t>(std::ceil(position));
    const double fraction = position - static_cast<double>(lowerIndex);
    const double lowerValue = values[lowerIndex];
    const double upperValue = values[upperIndex];
    return lowerValue + ((upperValue - lowerValue) * fraction);
}

void writeMetricSummary(std::ostream& output,
                        const std::string& name,
                        const MetricSamples& samples,
                        int indentLevel,
                        bool trailingComma) {
    const auto& values = samples.values;
    writeIndent(output, indentLevel);
    output << '"' << escapeJsonString(name) << "\": {\n";

    if (values.empty()) {
        writeIndent(output, indentLevel + 1);
        output << "\"count\": 0\n";
        writeIndent(output, indentLevel);
        output << '}' << (trailingComma ? ",\n" : "\n");
        return;
    }

    const auto minMax = std::minmax_element(values.begin(), values.end());
    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    const double mean = sum / static_cast<double>(values.size());
    double varianceSum = 0.0;
    for (const double value : values) {
        const double delta = value - mean;
        varianceSum += delta * delta;
    }

    const double variance = varianceSum / static_cast<double>(values.size());

    writeIndent(output, indentLevel + 1);
    output << "\"count\": " << values.size() << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"mean\": " << formatDouble(mean) << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"stddev\": " << formatDouble(std::sqrt(variance)) << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"min\": " << formatDouble(*minMax.first) << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"max\": " << formatDouble(*minMax.second) << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"q25\": " << formatDouble(quantile(values, 0.25)) << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"median\": " << formatDouble(quantile(values, 0.50)) << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"q75\": " << formatDouble(quantile(values, 0.75)) << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"q90\": " << formatDouble(quantile(values, 0.90)) << "\n";
    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

template <typename MapType>
void writeCountMap(std::ostream& output,
                   const MapType& values,
                   int indentLevel,
                   bool trailingComma) {
    output << "{\n";
    std::size_t remaining = values.size();
    for (const auto& entry : values) {
        writeIndent(output, indentLevel + 1);
        output << '"' << escapeJsonString(toJsonObjectKey(entry.first)) << "\": " << entry.second;
        output << (--remaining > 0 ? ",\n" : "\n");
    }
    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

template <typename MapType>
void writeDoubleMap(std::ostream& output,
                    const MapType& values,
                    int indentLevel,
                    bool trailingComma) {
    output << "{\n";
    std::size_t remaining = values.size();
    for (const auto& entry : values) {
        writeIndent(output, indentLevel + 1);
        output << '"' << escapeJsonString(toJsonObjectKey(entry.first)) << "\": "
               << formatDouble(entry.second);
        output << (--remaining > 0 ? ",\n" : "\n");
    }
    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

void writeArrayHistogram(std::ostream& output,
                         const std::array<std::uint64_t, 4>& values,
                         int indentLevel,
                         bool trailingComma) {
    output << "{\n";
    for (std::size_t index = 0; index < values.size(); ++index) {
        writeIndent(output, indentLevel + 1);
        output << '"' << index << "\": " << values[index];
        output << (index + 1 < values.size() ? ",\n" : "\n");
    }
    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

void writeBrightnessHistogram(std::ostream& output,
                              const std::array<std::uint64_t, kBrightnessBinCount>& values,
                              int indentLevel,
                              bool trailingComma) {
    output << "[\n";
    for (std::size_t index = 0; index < values.size(); ++index) {
        const int binStart = static_cast<int>(index) * (256 / kBrightnessBinCount);
        const int binEnd = (static_cast<int>(index) + 1) * (256 / kBrightnessBinCount) - 1;
        writeIndent(output, indentLevel + 1);
        output << "{\"binStart\": " << binStart
               << ", \"binEnd\": " << binEnd
               << ", \"count\": " << values[index] << '}';
        output << (index + 1 < values.size() ? ",\n" : "\n");
    }
    writeIndent(output, indentLevel);
    output << ']' << (trailingComma ? ",\n" : "\n");
}

void writeExamples(std::ostream& output,
                   const std::vector<BoardExample>& examples,
                   int indentLevel,
                   bool trailingComma) {
    output << "[\n";
    for (std::size_t index = 0; index < examples.size(); ++index) {
        const BoardExample& example = examples[index];
        writeIndent(output, indentLevel + 1);
        output << "{\n";
        writeIndent(output, indentLevel + 2);
        output << "\"seed\": " << example.seed << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"validCells\": " << example.validCells << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"grassCells\": " << example.grassCells << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"dirtCells\": " << example.dirtCells << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"waterCells\": " << example.waterCells << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"waterComponentCount\": " << example.waterComponentCount << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"largestWaterComponent\": " << example.largestWaterComponent << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"playerAiSpawnDistance\": " << formatDouble(example.playerAiSpawnDistance) << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"mineToLakeDistance\": " << formatDouble(example.mineToLakeDistance) << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"farmToLakeDistance\": " << formatDouble(example.farmToLakeDistance) << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"mineToFarmDistance\": " << formatDouble(example.mineToFarmDistance) << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"playerSpawn\": {\"x\": " << example.playerSpawn.x << ", \"y\": " << example.playerSpawn.y << "},\n";
        writeIndent(output, indentLevel + 2);
        output << "\"aiSpawn\": {\"x\": " << example.aiSpawn.x << ", \"y\": " << example.aiSpawn.y << "}\n";
        writeIndent(output, indentLevel + 1);
        output << '}';
        output << (index + 1 < examples.size() ? ",\n" : "\n");
    }
    writeIndent(output, indentLevel);
    output << ']' << (trailingComma ? ",\n" : "\n");
}

void writeNestedArrayMap(std::ostream& output,
                         const std::map<std::string, std::array<std::uint64_t, 4>>& values,
                         int indentLevel,
                         bool trailingComma) {
    output << "{\n";
    std::size_t remaining = values.size();
    for (const auto& entry : values) {
        writeIndent(output, indentLevel + 1);
        output << '"' << escapeJsonString(entry.first) << "\": ";
        writeArrayHistogram(output, entry.second, indentLevel + 1, --remaining > 0);
    }
    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

void writeNestedBrightnessMap(
    std::ostream& output,
    const std::map<std::string, std::array<std::uint64_t, kBrightnessBinCount>>& values,
    int indentLevel,
    bool trailingComma) {
    output << "{\n";
    std::size_t remaining = values.size();
    for (const auto& entry : values) {
        writeIndent(output, indentLevel + 1);
        output << '"' << escapeJsonString(entry.first) << "\": ";
        writeBrightnessHistogram(output, entry.second, indentLevel + 1, --remaining > 0);
    }
    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

GameConfig loadRuntimeConfig() {
    GameConfig config;
    if (config.loadFromFile("assets/config/master_config.json")) {
        return config;
    }

    if (config.loadFromFile("assets/config/game_params.json")) {
        return config;
    }

    throw std::runtime_error("Failed to load assets/config/master_config.json or assets/config/game_params.json.");
}

void recordTerrainHistograms(const Board& board, MapSuiteResults& results) {
    const int diameter = board.getDiameter();
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle || cell.type == CellType::Void) {
                continue;
            }

            const std::string typeName = cellTypeName(cell.type);
            ++results.terrainFlipHistogram[typeName][std::clamp(cell.terrainFlipMask, 0, 3)];
            ++results.brightnessHistogram[typeName][brightnessBin(cell.terrainBrightness)];
        }
    }
}

void recordBuildingHistograms(const std::vector<Building>& publicBuildings,
                              MapSuiteResults& results) {
    for (const Building& building : publicBuildings) {
        const std::string typeName = buildingTypeName(building.type);
        ++results.buildingRotationHistogram[typeName][std::clamp(building.rotationQuarterTurns, 0, 3)];
        ++results.buildingFlipHistogram[typeName][std::clamp(building.flipMask, 0, 3)];
        if (building.type == BuildingType::Mine) {
            recordHeatmap(results.mineOriginHeatmap, building.origin);
        } else if (building.type == BuildingType::Farm) {
            recordHeatmap(results.farmOriginHeatmap, building.origin);
        }
    }
}

BoardExample analyzeBoard(std::uint32_t seed,
                          const Board& board,
                          const GenerationResult& generation,
                          const std::vector<Building>& publicBuildings,
                          MapSuiteResults& results) {
    BoardExample example;
    example.seed = seed;
    example.playerSpawn = generation.playerSpawn;
    example.aiSpawn = generation.aiSpawn;

    recordHeatmap(results.playerSpawnHeatmap, generation.playerSpawn);
    recordHeatmap(results.aiSpawnHeatmap, generation.aiSpawn);

    const int diameter = board.getDiameter();
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle) {
                continue;
            }

            ++example.validCells;
            switch (cell.type) {
            case CellType::Grass:
                ++example.grassCells;
                break;
            case CellType::Dirt:
                ++example.dirtCells;
                break;
            case CellType::Water:
                ++example.waterCells;
                break;
            case CellType::Void:
                break;
            }
        }
    }

    const std::vector<WaterComponent> waterComponents = findWaterComponents(board);
    example.waterComponentCount = static_cast<int>(waterComponents.size());
    for (const WaterComponent& component : waterComponents) {
        const int size = static_cast<int>(component.cells.size());
        example.largestWaterComponent = std::max(example.largestWaterComponent, size);
        ++results.waterComponentSizeHistogram[size];
    }

    std::vector<std::vector<sf::Vector2i>> waterSets;
    waterSets.reserve(waterComponents.size());
    for (const WaterComponent& component : waterComponents) {
        waterSets.push_back(component.cells);
    }

    std::vector<std::vector<sf::Vector2i>> mineCells;
    std::vector<std::vector<sf::Vector2i>> farmCells;
    for (const Building& building : publicBuildings) {
        if (building.type == BuildingType::Mine) {
            mineCells.push_back(building.getOccupiedCells());
        } else if (building.type == BuildingType::Farm) {
            farmCells.push_back(building.getOccupiedCells());
        }
    }

    example.playerAiSpawnDistance = euclideanDistance(generation.playerSpawn, generation.aiSpawn);
    example.mineToLakeDistance = meanMinimumDistance(mineCells, waterSets);
    example.farmToLakeDistance = meanMinimumDistance(farmCells, waterSets);
    example.mineToFarmDistance = meanMinimumDistance(mineCells, farmCells);
    return example;
}

MapSuiteResults runMapSuite(const GameConfig& config, const BatchOptions& options) {
    MapSuiteResults results;
    results.terrainFlipHistogram["Grass"] = {0, 0, 0, 0};
    results.terrainFlipHistogram["Dirt"] = {0, 0, 0, 0};
    results.terrainFlipHistogram["Water"] = {0, 0, 0, 0};
    results.brightnessHistogram["Grass"] = {};
    results.brightnessHistogram["Dirt"] = {};
    results.brightnessHistogram["Water"] = {};

    for (int sampleIndex = 0; sampleIndex < options.sampleCount; ++sampleIndex) {
        const std::uint32_t seed = options.baseSeed + static_cast<std::uint32_t>(sampleIndex);

        Board board;
        board.init(config.getMapRadius());
        std::vector<Building> publicBuildings;
        const GenerationResult generation = BoardGenerator::generate(board, config, publicBuildings, seed);

        recordTerrainHistograms(board, results);
        recordBuildingHistograms(publicBuildings, results);

        const BoardExample example = analyzeBoard(seed, board, generation, publicBuildings, results);
        results.validCells.add(static_cast<double>(example.validCells));
        results.grassCells.add(static_cast<double>(example.grassCells));
        results.dirtCells.add(static_cast<double>(example.dirtCells));
        results.waterCells.add(static_cast<double>(example.waterCells));
        results.waterComponentCount.add(static_cast<double>(example.waterComponentCount));
        results.largestWaterComponent.add(static_cast<double>(example.largestWaterComponent));
        results.playerAiSpawnDistance.add(example.playerAiSpawnDistance);
        results.mineToLakeDistance.add(example.mineToLakeDistance);
        results.farmToLakeDistance.add(example.farmToLakeDistance);
        results.mineToFarmDistance.add(example.mineToFarmDistance);
        results.dirtCoveragePercent.add(
            example.validCells > 0
                ? (static_cast<double>(example.dirtCells) * 100.0) / static_cast<double>(example.validCells)
                : 0.0);
        results.waterCoveragePercent.add(
            example.validCells > 0
                ? (static_cast<double>(example.waterCells) * 100.0) / static_cast<double>(example.validCells)
                : 0.0);

        if (static_cast<int>(results.examples.size()) < kExampleSampleCount) {
            results.examples.push_back(example);
        }
    }

    return results;
}

void recordChestPhaseReward(ChestRewardPhaseResults& phase, const ChestReward& reward) {
    ++phase.rewardCount;
    ++phase.rewardTypeCounts[chestRewardTypeLabel(reward.type)];
    if (reward.type == ChestRewardType::Gold) {
        phase.goldAmounts.add(static_cast<double>(reward.amount));
        ++phase.goldAmountHistogram[reward.amount];
    }
}

std::map<std::string, double> expectedChestRewardProbabilities(const GameConfig& config, bool lateGame) {
    const int goldWeight = lateGame ? config.getChestLateGoldWeight() : config.getChestEarlyGoldWeight();
    const int movementWeight = lateGame
        ? config.getChestLateMovementBonusWeight()
        : config.getChestEarlyMovementBonusWeight();
    const int buildWeight = lateGame
        ? config.getChestLateBuildBonusWeight()
        : config.getChestEarlyBuildBonusWeight();
    const int total = goldWeight + movementWeight + buildWeight;

    if (total <= 0) {
        return {
            {chestRewardTypeLabel(ChestRewardType::Gold), 1.0},
            {chestRewardTypeLabel(ChestRewardType::MovementPointsMaxBonus), 0.0},
            {chestRewardTypeLabel(ChestRewardType::BuildPointsMaxBonus), 0.0}};
    }

    return {
        {chestRewardTypeLabel(ChestRewardType::Gold), static_cast<double>(goldWeight) / static_cast<double>(total)},
        {chestRewardTypeLabel(ChestRewardType::MovementPointsMaxBonus), static_cast<double>(movementWeight) / static_cast<double>(total)},
        {chestRewardTypeLabel(ChestRewardType::BuildPointsMaxBonus), static_cast<double>(buildWeight) / static_cast<double>(total)}};
}

XPSuiteResults runXPSuite(const GameConfig& config, const BatchOptions& options) {
    XPSuiteResults results;
    const std::array<XPRewardSource, 7> sources{
        XPRewardSource::KillPawn,
        XPRewardSource::KillKnight,
        XPRewardSource::KillBishop,
        XPRewardSource::KillRook,
        XPRewardSource::KillQueen,
        XPRewardSource::DestroyBlock,
        XPRewardSource::ArenaPerTurn};

    for (const XPRewardSource source : sources) {
        XPSourceSuiteResults sourceResults;
        sourceResults.profile = config.getXPRewardProfile(source);

        for (int sampleIndex = 0; sampleIndex < options.sampleCount; ++sampleIndex) {
            XPSystemState state{};
            const std::uint32_t seed = options.baseSeed + static_cast<std::uint32_t>(sampleIndex);
            const int amount = XPSystem::sampleReward(source, state, seed, config);
            sourceResults.amounts.add(static_cast<double>(amount));
            ++sourceResults.amountHistogram[amount];
        }

        results.sources.emplace(xpRewardSourceName(source), std::move(sourceResults));
    }

    return results;
}

ChestSuiteResults runChestSuite(const GameConfig& config, const BatchOptions& options) {
    ChestSuiteResults results;

    const int lateTurn = std::max(1, config.getChestLateGameTurn());
    const int earlyTurn = std::max(1, lateTurn - 1);

    for (int sampleIndex = 0; sampleIndex < options.sampleCount; ++sampleIndex) {
        const std::uint32_t seed = options.baseSeed + static_cast<std::uint32_t>(sampleIndex);

        ChestSystemState delayState{};
        ChestSystem::scheduleNextSpawn(delayState, seed, 0, config);
        results.spawnDelayTurns.add(static_cast<double>(delayState.nextSpawnTurn));
        ++results.spawnDelayHistogram[delayState.nextSpawnTurn];

        {
            ChestLootProgressionState progressionState{};
            std::uint32_t rewardCounter = 0;
            const ChestReward reward = ChestLootProgression::resolveReward(
                progressionState,
                rewardCounter,
                KingdomId::White,
                seed,
                earlyTurn,
                config).reward;
            recordChestPhaseReward(results.earlyPhase, reward);
        }

        {
            ChestLootProgressionState progressionState{};
            std::uint32_t rewardCounter = 0;
            const ChestReward reward = ChestLootProgression::resolveReward(
                progressionState,
                rewardCounter,
                KingdomId::White,
                seed,
                lateTurn,
                config).reward;
            recordChestPhaseReward(results.latePhase, reward);
        }

        ChestSystemState timelineState{};
        ChestSystem::initialize(timelineState, seed, 1, config);

        int collectedChests = 0;
        int totalGold = 0;
        int totalMovementBonus = 0;
        int totalBuildBonus = 0;
        for (int turn = 1; turn <= options.turnBudget; ++turn) {
            if (turn < timelineState.nextSpawnTurn) {
                continue;
            }

            const ChestReward reward = ChestLootProgression::resolveReward(
                timelineState.lootProgression,
                timelineState.rewardRngCounter,
                KingdomId::White,
                seed,
                turn,
                config).reward;
            ++collectedChests;
            switch (reward.type) {
            case ChestRewardType::Gold:
                totalGold += reward.amount;
                break;
            case ChestRewardType::MovementPointsMaxBonus:
                totalMovementBonus += reward.amount;
                break;
            case ChestRewardType::BuildPointsMaxBonus:
                totalBuildBonus += reward.amount;
                break;
            }

            ChestSystem::scheduleNextSpawn(timelineState, seed, turn, config);
        }

        results.immediateCollectionTimeline.collectedChests.add(static_cast<double>(collectedChests));
        results.immediateCollectionTimeline.totalGold.add(static_cast<double>(totalGold));
        results.immediateCollectionTimeline.totalMovementBonus.add(static_cast<double>(totalMovementBonus));
        results.immediateCollectionTimeline.totalBuildBonus.add(static_cast<double>(totalBuildBonus));
    }

    return results;
}

struct WeatherMaskSummary {
    int visibleCells = 0;
    double visibleCoveragePercent = 0.0;
    double meanAlpha = 0.0;
    int maxAlpha = 0;
};

WeatherMaskSummary summarizeIsolatedFront(const Board& board,
                                          const WeatherFrontDescriptor& front,
                                          const GameConfig& config) {
    WeatherMaskSummary summary;
    const std::vector<sf::Vector2i> validCells = board.getAllValidCells();
    for (int frontStep = 0; frontStep < front.totalTurnSteps; ++frontStep) {
        WeatherFrontDescriptor sampledFront = front;
        sampledFront.currentTurnStep = frontStep;

        WeatherSystemState isolatedState{};
        isolatedState.hasActiveFront = true;
        isolatedState.activeFront = sampledFront;
        isolatedState.activeFronts.push_back(sampledFront);
        isolatedState.revision = static_cast<std::uint32_t>(frontStep + 1);

        WeatherMaskCache cache;
        WeatherSystem::rebuildMask(board, isolatedState, config, cache);

        int visibleCells = 0;
        int maxAlpha = 0;
        double alphaSum = 0.0;
        for (const sf::Vector2i& position : validCells) {
            const int alpha = static_cast<int>(WeatherSystem::alphaAtCell(cache, position.x, position.y));
            if (alpha <= 0) {
                continue;
            }

            ++visibleCells;
            maxAlpha = std::max(maxAlpha, alpha);
            alphaSum += static_cast<double>(alpha);
        }

        if (visibleCells <= summary.visibleCells) {
            continue;
        }

        summary.visibleCells = visibleCells;
        summary.maxAlpha = maxAlpha;
        summary.visibleCoveragePercent = validCells.empty()
            ? 0.0
            : (static_cast<double>(visibleCells) * 100.0) / static_cast<double>(validCells.size());
        summary.meanAlpha = visibleCells > 0 ? alphaSum / static_cast<double>(visibleCells) : 0.0;
    }

    return summary;
}

WeatherSuiteResults runWeatherSuite(const GameConfig& config, const BatchOptions& options) {
    WeatherSuiteResults results;
    const int totalTurnSteps = options.turnBudget * WeatherSystem::kStepsPerTurn;

    for (int sampleIndex = 0; sampleIndex < options.sampleCount; ++sampleIndex) {
        const std::uint32_t seed = options.baseSeed + static_cast<std::uint32_t>(sampleIndex);

        WeatherSystemState arrivalState{};
        WeatherSystem::initialize(arrivalState, seed, 0, config);
        const int arrivalDelayTurns = std::max(1, arrivalState.nextSpawnTurnStep / WeatherSystem::kStepsPerTurn);
        results.arrivalDelayTurns.add(static_cast<double>(arrivalDelayTurns));
        ++results.arrivalDelayHistogram[arrivalDelayTurns];

        Board board;
        board.init(config.getMapRadius());

        WeatherSystemState state{};
        WeatherSystem::initialize(state, seed, 0, config);
        int previousSpawnStep = -1;
        int spawnedFrontCount = 0;
        int maxActiveFronts = 0;

        for (int step = 0; step < totalTurnSteps; ++step) {
            if (WeatherSystem::trySpawnFront(state, board, seed, step, config) && !state.activeFronts.empty()) {
                const WeatherFrontDescriptor& front = state.activeFronts.back();
                ++spawnedFrontCount;

                const std::string directionLabel = weatherDirectionName(front.direction);
                ++results.directionCounts[directionLabel];

                const double aspectRatio = static_cast<double>(front.radiusAcrossTimes1000)
                    / static_cast<double>(std::max(1, front.radiusAlongTimes1000));
                const double durationTurns = static_cast<double>(front.totalTurnSteps)
                    / static_cast<double>(WeatherSystem::kStepsPerTurn);

                results.aspectRatio.add(aspectRatio);
                results.durationTurns.add(durationTurns);
                ++results.durationHistogram[static_cast<int>(std::lround(durationTurns))];

                const WeatherMaskSummary maskSummary = summarizeIsolatedFront(board, front, config);
                results.coveragePercent.add(maskSummary.visibleCoveragePercent);
                ++results.coverageHistogram[static_cast<int>(std::lround(maskSummary.visibleCoveragePercent))];
                results.isolatedVisibleCellCount.add(static_cast<double>(maskSummary.visibleCells));
                results.isolatedVisibleCoveragePercent.add(maskSummary.visibleCoveragePercent);
                results.isolatedMeanAlpha.add(maskSummary.meanAlpha);
                results.isolatedMaxAlpha.add(static_cast<double>(maskSummary.maxAlpha));

                if (previousSpawnStep >= 0) {
                    results.spawnIntervalTurns.add(
                        static_cast<double>(step - previousSpawnStep)
                        / static_cast<double>(WeatherSystem::kStepsPerTurn));
                }
                previousSpawnStep = step;

                if (static_cast<int>(results.examples.size()) < kExampleSampleCount) {
                    results.examples.push_back(WeatherExample{
                        seed,
                        directionLabel,
                        step,
                        front.totalTurnSteps,
                        aspectRatio,
                        maskSummary.visibleCoveragePercent,
                        maskSummary.meanAlpha,
                        maskSummary.maxAlpha,
                        static_cast<int>(state.activeFronts.size())});
                }
            }

            const int activeFrontCount = static_cast<int>(state.activeFronts.size());
            results.activeFrontCountPerStep.add(static_cast<double>(activeFrontCount));
            ++results.activeFrontCountHistogram[activeFrontCount];
            maxActiveFronts = std::max(maxActiveFronts, activeFrontCount);

            WeatherSystem::advanceFront(state, seed, step, config);
        }

        results.maxActiveFrontsPerWorld.add(static_cast<double>(maxActiveFronts));
        results.spawnedFrontCountPerWorld.add(static_cast<double>(spawnedFrontCount));
    }

    return results;
}

void writeRunManifest(std::ostream& output,
                      const GameConfig& config,
                      const BatchOptions& options,
                      int indentLevel,
                      bool trailingComma) {
    writeIndent(output, indentLevel);
    output << "\"run\": {\n";
    writeIndent(output, indentLevel + 1);
    output << "\"generator\": \"ANormalChessGameBatchStats\",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"suite\": \"randomness_statistics\",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"sampleCount\": " << options.sampleCount << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"turnBudget\": " << options.turnBudget << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"baseSeed\": " << options.baseSeed << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"config\": {\n";
    writeIndent(output, indentLevel + 2);
    output << "\"mapRadius\": " << config.getMapRadius() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"numMines\": " << config.getNumMines() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"numFarms\": " << config.getNumFarms() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"minPublicBuildingDistance\": " << config.getMinPublicBuildingDistance() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"playerSpawnZonePercent\": " << config.getPlayerSpawnZonePercent() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"aiSpawnZonePercent\": " << config.getAISpawnZonePercent() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"terrainNoiseScale\": " << config.getTerrainNoiseScale() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"terrainOctaves\": " << config.getTerrainOctaves() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"dirtCoveragePercent\": " << config.getDirtCoveragePercent() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"waterCoveragePercent\": " << config.getWaterCoveragePercent() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"numLakes\": " << config.getNumLakes() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"lakeMinRadius\": " << config.getLakeMinRadius() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"lakeMaxRadius\": " << config.getLakeMaxRadius() << "\n";
    writeIndent(output, indentLevel + 1);
    output << "}\n";
    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

void writeMapSuite(std::ostream& output,
                   const MapSuiteResults& results,
                   int indentLevel,
                   bool trailingComma) {
    writeIndent(output, indentLevel);
    output << "\"map_generation\": {\n";

    writeIndent(output, indentLevel + 1);
    output << "\"summary\": {\n";
    writeMetricSummary(output, "valid_cells", results.validCells, indentLevel + 2, true);
    writeMetricSummary(output, "grass_cells", results.grassCells, indentLevel + 2, true);
    writeMetricSummary(output, "dirt_cells", results.dirtCells, indentLevel + 2, true);
    writeMetricSummary(output, "water_cells", results.waterCells, indentLevel + 2, true);
    writeMetricSummary(output, "dirt_coverage_percent", results.dirtCoveragePercent, indentLevel + 2, true);
    writeMetricSummary(output, "water_coverage_percent", results.waterCoveragePercent, indentLevel + 2, true);
    writeMetricSummary(output, "water_component_count", results.waterComponentCount, indentLevel + 2, true);
    writeMetricSummary(output, "largest_water_component", results.largestWaterComponent, indentLevel + 2, true);
    writeMetricSummary(output, "player_ai_spawn_distance", results.playerAiSpawnDistance, indentLevel + 2, true);
    writeMetricSummary(output, "mine_to_nearest_lake_distance", results.mineToLakeDistance, indentLevel + 2, true);
    writeMetricSummary(output, "farm_to_nearest_lake_distance", results.farmToLakeDistance, indentLevel + 2, true);
    writeMetricSummary(output, "mine_to_nearest_farm_distance", results.mineToFarmDistance, indentLevel + 2, false);
    writeIndent(output, indentLevel + 1);
    output << "},\n";

    writeIndent(output, indentLevel + 1);
    output << "\"histograms\": {\n";
    writeIndent(output, indentLevel + 2);
    output << "\"water_component_size\": ";
    writeCountMap(output, results.waterComponentSizeHistogram, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"terrain_flip_by_type\": ";
    writeNestedArrayMap(output, results.terrainFlipHistogram, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"terrain_brightness_by_type\": ";
    writeNestedBrightnessMap(output, results.brightnessHistogram, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"public_building_rotation_by_type\": ";
    writeNestedArrayMap(output, results.buildingRotationHistogram, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"public_building_flip_by_type\": ";
    writeNestedArrayMap(output, results.buildingFlipHistogram, indentLevel + 2, false);
    writeIndent(output, indentLevel + 1);
    output << "},\n";

    writeIndent(output, indentLevel + 1);
    output << "\"heatmaps\": {\n";
    writeIndent(output, indentLevel + 2);
    output << "\"player_spawn\": ";
    writeCountMap(output, results.playerSpawnHeatmap, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"ai_spawn\": ";
    writeCountMap(output, results.aiSpawnHeatmap, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"mine_origins\": ";
    writeCountMap(output, results.mineOriginHeatmap, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"farm_origins\": ";
    writeCountMap(output, results.farmOriginHeatmap, indentLevel + 2, false);
    writeIndent(output, indentLevel + 1);
    output << "},\n";

    writeIndent(output, indentLevel + 1);
    output << "\"examples\": ";
    writeExamples(output, results.examples, indentLevel + 1, false);

    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

void writeXPSuite(std::ostream& output,
                  const XPSuiteResults& results,
                  int indentLevel,
                  bool trailingComma) {
    writeIndent(output, indentLevel);
    output << "\"xp_rewards\": {\n";
    writeIndent(output, indentLevel + 1);
    output << "\"sources\": {\n";

    std::size_t remainingSources = results.sources.size();
    for (const auto& entry : results.sources) {
        const XPSourceSuiteResults& sourceResults = entry.second;
        writeIndent(output, indentLevel + 2);
        output << '"' << entry.first << "\": {\n";

        writeIndent(output, indentLevel + 3);
        output << "\"summary\": {\n";
        writeMetricSummary(output, "amount", sourceResults.amounts, indentLevel + 4, false);
        writeIndent(output, indentLevel + 3);
        output << "},\n";

        writeIndent(output, indentLevel + 3);
        output << "\"expected\": {\n";
        writeIndent(output, indentLevel + 4);
        output << "\"inputMean\": " << sourceResults.profile.mean << ",\n";
        writeIndent(output, indentLevel + 4);
        output << "\"inputVariance\": " << formatDouble(std::pow(xpProfileSigma(sourceResults.profile), 2.0)) << ",\n";
        writeIndent(output, indentLevel + 4);
        output << "\"sigma\": " << formatDouble(xpProfileSigma(sourceResults.profile)) << ",\n";
        writeIndent(output, indentLevel + 4);
        output << "\"minimum\": " << sourceResults.profile.minimum << ",\n";
        writeIndent(output, indentLevel + 4);
        output << "\"clampMin\": "
               << formatDouble(std::max(static_cast<double>(sourceResults.profile.minimum),
                    static_cast<double>(sourceResults.profile.mean) - xpProfileClampDelta(sourceResults.profile)))
               << ",\n";
        writeIndent(output, indentLevel + 4);
        output << "\"clampMax\": "
               << formatDouble(std::max(
                    std::max(static_cast<double>(sourceResults.profile.minimum),
                        static_cast<double>(sourceResults.profile.mean) - xpProfileClampDelta(sourceResults.profile)),
                    static_cast<double>(sourceResults.profile.mean) + xpProfileClampDelta(sourceResults.profile)))
               << "\n";
        writeIndent(output, indentLevel + 3);
        output << "},\n";

        writeIndent(output, indentLevel + 3);
        output << "\"histogram\": ";
        writeCountMap(output, sourceResults.amountHistogram, indentLevel + 3, false);

        writeIndent(output, indentLevel + 2);
        output << '}' << (--remainingSources > 0 ? ",\n" : "\n");
    }

    writeIndent(output, indentLevel + 1);
    output << "}\n";
    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

void writeChestRewardPhase(std::ostream& output,
                           const std::string& phaseName,
                           const ChestRewardPhaseResults& results,
                           const std::map<std::string, double>& expectedProbabilities,
                           int indentLevel,
                           bool trailingComma) {
    writeIndent(output, indentLevel);
    output << '"' << phaseName << "\": {\n";
    writeIndent(output, indentLevel + 1);
    output << "\"rewardCount\": " << results.rewardCount << ",\n";
    writeIndent(output, indentLevel + 1);
    output << "\"rewardTypeCounts\": ";
    writeCountMap(output, results.rewardTypeCounts, indentLevel + 1, true);
    writeIndent(output, indentLevel + 1);
    output << "\"observedRewardTypeProbabilities\": ";
    writeDoubleMap(output, probabilityMapFromCounts(results.rewardTypeCounts), indentLevel + 1, true);
    writeIndent(output, indentLevel + 1);
    output << "\"expectedRewardTypeProbabilities\": ";
    writeDoubleMap(output, expectedProbabilities, indentLevel + 1, true);
    writeIndent(output, indentLevel + 1);
    output << "\"goldAmountSummary\": {\n";
    writeMetricSummary(output, "gold_amount", results.goldAmounts, indentLevel + 2, false);
    writeIndent(output, indentLevel + 1);
    output << "},\n";
    writeIndent(output, indentLevel + 1);
    output << "\"goldAmountHistogram\": ";
    writeCountMap(output, results.goldAmountHistogram, indentLevel + 1, false);
    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

void writeChestSuite(std::ostream& output,
                     const ChestSuiteResults& results,
                     const GameConfig& config,
                     const BatchOptions& options,
                     int indentLevel,
                     bool trailingComma) {
    writeIndent(output, indentLevel);
    output << "\"chest_system\": {\n";

    writeIndent(output, indentLevel + 1);
    output << "\"summary\": {\n";
    writeMetricSummary(output, "spawn_delay_turns", results.spawnDelayTurns, indentLevel + 2, true);
    writeMetricSummary(output, "timeline_collected_chests", results.immediateCollectionTimeline.collectedChests, indentLevel + 2, true);
    writeMetricSummary(output, "timeline_total_gold", results.immediateCollectionTimeline.totalGold, indentLevel + 2, true);
    writeMetricSummary(output, "timeline_total_movement_bonus", results.immediateCollectionTimeline.totalMovementBonus, indentLevel + 2, true);
    writeMetricSummary(output, "timeline_total_build_bonus", results.immediateCollectionTimeline.totalBuildBonus, indentLevel + 2, false);
    writeIndent(output, indentLevel + 1);
    output << "},\n";

    const double chestShape = std::max(0.10, static_cast<double>(config.getChestWeibullShapeTimes100()) / 100.0);
    const double chestScale = std::max(0.50, static_cast<double>(config.getChestWeibullScaleTurns()));

    writeIndent(output, indentLevel + 1);
    output << "\"expected\": {\n";
    writeIndent(output, indentLevel + 2);
    output << "\"spawnDelayContinuousReferenceMean\": " << formatDouble(weibullMean(chestShape, chestScale)) << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"spawnDelayContinuousReferenceVariance\": " << formatDouble(weibullVariance(chestShape, chestScale)) << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"spawnDelayCooldownFloor\": " << config.getChestRespawnCooldownTurns() << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"timelinePolicy\": \"immediate_collection_same_turn\",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"timelineTurnBudget\": " << options.turnBudget << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"goldInputMean\": " << config.getChestGoldRewardProfile().mean << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"goldInputVariance\": "
           << formatDouble(std::pow(xpProfileSigma(config.getChestGoldRewardProfile()), 2.0))
           << "\n";
    writeIndent(output, indentLevel + 1);
    output << "},\n";

    writeIndent(output, indentLevel + 1);
    output << "\"spawnDelayHistogram\": ";
    writeCountMap(output, results.spawnDelayHistogram, indentLevel + 1, true);

    writeIndent(output, indentLevel + 1);
    output << "\"rewardPhases\": {\n";
    writeChestRewardPhase(
        output,
        "early_phase",
        results.earlyPhase,
        expectedChestRewardProbabilities(config, false),
        indentLevel + 2,
        true);
    writeChestRewardPhase(
        output,
        "late_phase",
        results.latePhase,
        expectedChestRewardProbabilities(config, true),
        indentLevel + 2,
        false);
    writeIndent(output, indentLevel + 1);
    output << "}\n";

    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

void writeWeatherExamples(std::ostream& output,
                          const std::vector<WeatherExample>& examples,
                          int indentLevel,
                          bool trailingComma) {
    output << "[\n";
    for (std::size_t index = 0; index < examples.size(); ++index) {
        const WeatherExample& example = examples[index];
        writeIndent(output, indentLevel + 1);
        output << "{\n";
        writeIndent(output, indentLevel + 2);
        output << "\"seed\": " << example.seed << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"direction\": \"" << example.direction << "\",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"spawnStep\": " << example.spawnStep << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"durationTurnSteps\": " << example.durationTurnSteps << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"aspectRatio\": " << formatDouble(example.aspectRatio) << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"visibleCoveragePercent\": " << formatDouble(example.visibleCoveragePercent) << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"meanAlpha\": " << formatDouble(example.meanAlpha) << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"maxAlpha\": " << example.maxAlpha << ",\n";
        writeIndent(output, indentLevel + 2);
        output << "\"simultaneousFrontsAtSpawn\": " << example.simultaneousFrontsAtSpawn << "\n";
        writeIndent(output, indentLevel + 1);
        output << '}';
        output << (index + 1 < examples.size() ? ",\n" : "\n");
    }
    writeIndent(output, indentLevel);
    output << ']' << (trailingComma ? ",\n" : "\n");
}

void writeWeatherSuite(std::ostream& output,
                       const WeatherSuiteResults& results,
                       const GameConfig& config,
                       int indentLevel,
                       bool trailingComma) {
    writeIndent(output, indentLevel);
    output << "\"weather_system\": {\n";

    writeIndent(output, indentLevel + 1);
    output << "\"summary\": {\n";
    writeMetricSummary(output, "arrival_delay_turns", results.arrivalDelayTurns, indentLevel + 2, true);
    writeMetricSummary(output, "spawn_interval_turns", results.spawnIntervalTurns, indentLevel + 2, true);
    writeMetricSummary(output, "active_front_count_per_step", results.activeFrontCountPerStep, indentLevel + 2, true);
    writeMetricSummary(output, "max_active_fronts_per_world", results.maxActiveFrontsPerWorld, indentLevel + 2, true);
    writeMetricSummary(output, "spawned_fronts_per_world", results.spawnedFrontCountPerWorld, indentLevel + 2, true);
    writeMetricSummary(output, "isolated_peak_visible_coverage_percent", results.isolatedVisibleCoveragePercent, indentLevel + 2, true);
    writeMetricSummary(output, "isolated_peak_visible_cell_count", results.isolatedVisibleCellCount, indentLevel + 2, true);
    writeMetricSummary(output, "isolated_peak_mean_alpha", results.isolatedMeanAlpha, indentLevel + 2, true);
    writeMetricSummary(output, "isolated_peak_max_alpha", results.isolatedMaxAlpha, indentLevel + 2, true);
    writeMetricSummary(output, "front_duration_turns", results.durationTurns, indentLevel + 2, true);
    writeMetricSummary(output, "front_aspect_ratio", results.aspectRatio, indentLevel + 2, false);
    writeIndent(output, indentLevel + 1);
    output << "},\n";

    const double arrivalShape = std::max(0.01, static_cast<double>(config.getWeatherArrivalGammaShapeTimes100()) / 100.0);
    const double arrivalScale = std::max(0.01, static_cast<double>(config.getWeatherArrivalGammaScaleTimes100()) / 100.0);
    const double durationShape = std::max(0.01, static_cast<double>(config.getWeatherDurationGammaShapeTimes100()) / 100.0);
    const double durationScale = std::max(0.01, static_cast<double>(config.getWeatherDurationGammaScaleTimes100()) / 100.0);

    writeIndent(output, indentLevel + 1);
    output << "\"expected\": {\n";
    writeIndent(output, indentLevel + 2);
    output << "\"arrivalContinuousReferenceMean\": "
           << formatDouble(gammaMean(arrivalShape, arrivalScale, static_cast<double>(config.getWeatherCooldownMinTurns())))
           << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"arrivalContinuousReferenceVariance\": " << formatDouble(gammaVariance(arrivalShape, arrivalScale)) << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"durationContinuousReferenceMean\": " << formatDouble(gammaMean(durationShape, durationScale)) << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"durationContinuousReferenceVariance\": " << formatDouble(gammaVariance(durationShape, durationScale)) << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"coverageUniformReferenceMean\": "
           << formatDouble(discreteUniformMean(config.getWeatherCoverageMinPercent(), config.getWeatherCoverageMaxPercent()))
           << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"coverageUniformReferenceVariance\": "
           << formatDouble(discreteUniformVariance(config.getWeatherCoverageMinPercent(), config.getWeatherCoverageMaxPercent()))
           << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"aspectRatioUniformReferenceMean\": "
           << formatDouble(discreteUniformMean(
                config.getWeatherAspectRatioMinTimes100(),
                config.getWeatherAspectRatioMaxTimes100(),
                0.01))
           << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"aspectRatioUniformReferenceVariance\": "
           << formatDouble(discreteUniformVariance(
                config.getWeatherAspectRatioMinTimes100(),
                config.getWeatherAspectRatioMaxTimes100(),
                0.01))
           << ",\n";
    writeIndent(output, indentLevel + 2);
    output << "\"directionProbabilities\": ";
    const std::array<int, kNumWeatherDirections> directionWeights = config.getWeatherDirectionWeights();
    std::map<std::string, double> directionProbabilities;
    const int totalDirectionWeight = std::accumulate(directionWeights.begin(), directionWeights.end(), 0);
    for (std::size_t index = 0; index < directionWeights.size(); ++index) {
        const WeatherDirection direction = static_cast<WeatherDirection>(index);
        directionProbabilities[weatherDirectionName(direction)] = totalDirectionWeight > 0
            ? static_cast<double>(directionWeights[index]) / static_cast<double>(totalDirectionWeight)
            : 0.0;
    }
    writeDoubleMap(output, directionProbabilities, indentLevel + 2, false);
    writeIndent(output, indentLevel + 1);
    output << "},\n";

    writeIndent(output, indentLevel + 1);
    output << "\"histograms\": {\n";
    writeIndent(output, indentLevel + 2);
    output << "\"arrival_delay_turns\": ";
    writeCountMap(output, results.arrivalDelayHistogram, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"duration_turns\": ";
    writeCountMap(output, results.durationHistogram, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"visible_coverage_percent\": ";
    writeCountMap(output, results.coverageHistogram, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"active_front_count\": ";
    writeCountMap(output, results.activeFrontCountHistogram, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"direction_counts\": ";
    writeCountMap(output, results.directionCounts, indentLevel + 2, true);
    writeIndent(output, indentLevel + 2);
    output << "\"direction_observed_probabilities\": ";
    writeDoubleMap(output, probabilityMapFromCounts(results.directionCounts), indentLevel + 2, false);
    writeIndent(output, indentLevel + 1);
    output << "},\n";

    writeIndent(output, indentLevel + 1);
    output << "\"examples\": ";
    writeWeatherExamples(output, results.examples, indentLevel + 1, false);

    writeIndent(output, indentLevel);
    output << '}' << (trailingComma ? ",\n" : "\n");
}

void writeJsonReport(const std::filesystem::path& outputPath,
                     const GameConfig& config,
                     const BatchOptions& options,
                     const MapSuiteResults& mapResults,
                     const XPSuiteResults& xpResults,
                     const ChestSuiteResults& chestResults,
                     const WeatherSuiteResults& weatherResults) {
    if (!outputPath.parent_path().empty()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }

    std::ofstream output(outputPath);
    if (!output) {
        throw std::runtime_error("Failed to open output file: " + outputPath.string());
    }

    output << "{\n";
    writeRunManifest(output, config, options, 1, true);
    writeIndent(output, 1);
    output << "\"suites\": {\n";
    writeMapSuite(output, mapResults, 2, true);
    writeXPSuite(output, xpResults, 2, true);
    writeChestSuite(output, chestResults, config, options, 2, true);
    writeWeatherSuite(output, weatherResults, config, 2, false);
    writeIndent(output, 1);
    output << "}\n";
    output << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        const BatchOptions options = parseArgs(argc, argv);
        const GameConfig config = loadRuntimeConfig();
        const MapSuiteResults mapResults = runMapSuite(config, options);
        const XPSuiteResults xpResults = runXPSuite(config, options);
        const ChestSuiteResults chestResults = runChestSuite(config, options);
        const WeatherSuiteResults weatherResults = runWeatherSuite(config, options);
        writeJsonReport(options.outputPath, config, options, mapResults, xpResults, chestResults, weatherResults);

        std::cout << "Wrote randomness statistics to " << options.outputPath.string() << "\n";
        std::cout << "Samples: " << options.sampleCount
                  << ", turnBudget: " << options.turnBudget
                  << ", baseSeed: " << options.baseSeed << "\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Batch statistics failed: " << exception.what() << "\n";
        return 1;
    }
}