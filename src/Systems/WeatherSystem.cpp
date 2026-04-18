#include "Systems/WeatherSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include <SFML/System/Vector2.hpp>

#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Config/GameConfig.hpp"

namespace {

constexpr int kFixedPointScale = 1000;
constexpr float kPi = 3.14159265358979323846f;

enum class EntryEdge {
    Top,
    Bottom,
    Left,
    Right
};

float fromFixed(int value) {
    return static_cast<float>(value) / static_cast<float>(kFixedPointScale);
}

int toFixed(float value) {
    return static_cast<int>(std::lround(value * static_cast<float>(kFixedPointScale)));
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

std::mt19937 makeEventGenerator(WeatherSystemState& state, std::uint32_t worldSeed) {
    const std::uint32_t baseSeed = (worldSeed == 0) ? 1u : worldSeed;
    return std::mt19937(mixSeed(baseSeed, state.rngCounter++));
}

float hashUnitFloat(std::uint32_t seed, int x, int y, std::uint32_t salt = 0u) {
    const std::uint32_t ux = static_cast<std::uint32_t>(x + 0x4000);
    const std::uint32_t uy = static_cast<std::uint32_t>(y + 0x4000);
    std::uint32_t mixed = mixSeed(seed, ux * 73856093u);
    mixed = mixSeed(mixed, uy * 19349663u);
    mixed = mixSeed(mixed, salt + 1u);
    return static_cast<float>(mixed & 0xffffu) / 65535.0f;
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

float valueNoise(std::uint32_t seed, float x, float y, int cellSpan) {
    const float safeSpan = static_cast<float>(std::max(1, cellSpan));
    const float scaledX = x / safeSpan;
    const float scaledY = y / safeSpan;
    const int x0 = static_cast<int>(std::floor(scaledX));
    const int y0 = static_cast<int>(std::floor(scaledY));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const float tx = smoothstep(scaledX - static_cast<float>(x0));
    const float ty = smoothstep(scaledY - static_cast<float>(y0));

    const float v00 = hashUnitFloat(seed, x0, y0);
    const float v10 = hashUnitFloat(seed, x1, y0);
    const float v01 = hashUnitFloat(seed, x0, y1);
    const float v11 = hashUnitFloat(seed, x1, y1);
    return lerp(lerp(v00, v10, tx), lerp(v01, v11, tx), ty);
}

float sampleLogNormalCell(std::uint32_t seed,
                          int cellX,
                          int cellY,
                          const GameConfig& config) {
    const double mu = static_cast<double>(config.getWeatherDensityMuTimes100()) / 100.0;
    const double sigma = std::max(
        0.01,
        static_cast<double>(config.getWeatherDensitySigmaTimes100()) / 100.0);
    std::mt19937 generator(mixSeed(seed, static_cast<std::uint32_t>(cellX + 0x5000) * 2246822519u));
    generator.seed(mixSeed(generator(), static_cast<std::uint32_t>(cellY + 0x5000) * 3266489917u));
    std::lognormal_distribution<double> distribution(mu, sigma);
    return static_cast<float>(distribution(generator));
}

float sampleGammaTurns(std::mt19937& generator,
                       int minimumTurns,
                       int shapeTimes100,
                       int scaleTimes100) {
    const double shape = std::max(0.01, static_cast<double>(shapeTimes100) / 100.0);
    const double scale = std::max(0.01, static_cast<double>(scaleTimes100) / 100.0);
    std::gamma_distribution<double> distribution(shape, scale);
    return static_cast<float>(minimumTurns + static_cast<int>(std::ceil(distribution(generator))));
}

EntryEdge randomElement(const std::array<EntryEdge, 2>& edges, std::mt19937& generator) {
    std::uniform_int_distribution<int> distribution(0, 1);
    return edges[distribution(generator)];
}

EntryEdge entryEdgeForDirection(WeatherDirection direction, std::mt19937& generator) {
    switch (direction) {
        case WeatherDirection::North:
            return EntryEdge::Bottom;
        case WeatherDirection::South:
            return EntryEdge::Top;
        case WeatherDirection::East:
            return EntryEdge::Left;
        case WeatherDirection::West:
            return EntryEdge::Right;
        case WeatherDirection::NorthEast:
            return randomElement({EntryEdge::Bottom, EntryEdge::Left}, generator);
        case WeatherDirection::NorthWest:
            return randomElement({EntryEdge::Bottom, EntryEdge::Right}, generator);
        case WeatherDirection::SouthEast:
            return randomElement({EntryEdge::Top, EntryEdge::Left}, generator);
        case WeatherDirection::SouthWest:
            return randomElement({EntryEdge::Top, EntryEdge::Right}, generator);
        case WeatherDirection::Count:
            break;
    }

    return EntryEdge::Left;
}

float sampleEdgePosition(std::mt19937& generator, int diameter, const GameConfig& config) {
    const double maxCoord = static_cast<double>(std::max(1, diameter - 1));
    const double centerWeight = std::max(1.0, static_cast<double>(config.getWeatherEntryCenterWeightTimes100()) / 100.0);
    const double cornerWeight = std::max(0.1, static_cast<double>(config.getWeatherEntryCornerWeightTimes100()) / 100.0);
    const std::array<double, 5> boundaries{
        0.0,
        maxCoord * 0.25,
        maxCoord * 0.5,
        maxCoord * 0.75,
        maxCoord};
    const std::array<double, 5> weights{
        cornerWeight,
        centerWeight,
        centerWeight * 1.1,
        centerWeight,
        cornerWeight};

    std::piecewise_linear_distribution<double> distribution(
        boundaries.begin(),
        boundaries.end(),
        weights.begin());
    return static_cast<float>(distribution(generator));
}

sf::Vector2f boundaryCenterForEdge(EntryEdge edge, float edgePosition, int diameter) {
    const float maxCoord = static_cast<float>(diameter) - 0.5f;
    switch (edge) {
        case EntryEdge::Top:
            return {edgePosition, -0.5f};
        case EntryEdge::Bottom:
            return {edgePosition, maxCoord};
        case EntryEdge::Left:
            return {-0.5f, edgePosition};
        case EntryEdge::Right:
            return {maxCoord, edgePosition};
    }

    return {-0.5f, edgePosition};
}

sf::Vector2f normalizeDirection(const std::array<int, 2>& directionStep) {
    const float dx = static_cast<float>(directionStep[0]);
    const float dy = static_cast<float>(directionStep[1]);
    const float length = std::sqrt((dx * dx) + (dy * dy));
    if (length <= 0.0f) {
        return {1.0f, 0.0f};
    }

    return {dx / length, dy / length};
}

float exitDistanceForRay(const sf::Vector2f& start,
                         const sf::Vector2f& direction,
                         int diameter) {
    const float minBound = -0.5f;
    const float maxBound = static_cast<float>(diameter) - 0.5f;
    float t = std::numeric_limits<float>::max();

    if (direction.x > 0.0f) {
        t = std::min(t, (maxBound - start.x) / direction.x);
    } else if (direction.x < 0.0f) {
        t = std::min(t, (minBound - start.x) / direction.x);
    }

    if (direction.y > 0.0f) {
        t = std::min(t, (maxBound - start.y) / direction.y);
    } else if (direction.y < 0.0f) {
        t = std::min(t, (minBound - start.y) / direction.y);
    }

    if (!std::isfinite(t) || t <= 0.0f) {
        return static_cast<float>(diameter);
    }

    return t;
}

float weatherDistancePerStep(const GameConfig& config) {
    const float blocksPerTurn = static_cast<float>(config.getWeatherSpeedBlocksPer100Turns()) / 100.0f;
    return std::max(0.001f, blocksPerTurn / static_cast<float>(WeatherSystem::kStepsPerTurn));
}

WeatherDirection sampleDirection(WeatherSystemState& state,
                                 std::uint32_t worldSeed,
                                 const GameConfig& config) {
    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    const std::array<int, kNumWeatherDirections> weights = config.getWeatherDirectionWeights();
    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    return static_cast<WeatherDirection>(distribution(generator));
}

void bumpRevision(WeatherSystemState& state) {
    ++state.revision;
}

float frontCenterX(const WeatherFrontDescriptor& descriptor) {
    return fromFixed(descriptor.centerStartXTimes1000)
        + (fromFixed(descriptor.stepXTimes1000) * static_cast<float>(descriptor.currentTurnStep));
}

float frontCenterY(const WeatherFrontDescriptor& descriptor) {
    return fromFixed(descriptor.centerStartYTimes1000)
        + (fromFixed(descriptor.stepYTimes1000) * static_cast<float>(descriptor.currentTurnStep));
}

float concealmentAlpha(const WeatherFrontDescriptor& front,
                       const Board& board,
                       int cellX,
                       int cellY,
                       const GameConfig& config) {
    if (!board.isInBounds(cellX, cellY)) {
        return 0.0f;
    }

    const Cell& cell = board.getCell(cellX, cellY);
    if (!cell.isInCircle) {
        return 0.0f;
    }

    const std::array<int, 2> directionStep = WeatherSystem::directionStep(front.direction);
    const sf::Vector2f forward = normalizeDirection(directionStep);
    const sf::Vector2f sideways{-forward.y, forward.x};
    const sf::Vector2f center{frontCenterX(front), frontCenterY(front)};
    const sf::Vector2f samplePoint{static_cast<float>(cellX), static_cast<float>(cellY)};
    const sf::Vector2f relative = samplePoint - center;

    const float along = (relative.x * forward.x) + (relative.y * forward.y);
    const float across = (relative.x * sideways.x) + (relative.y * sideways.y);
    const float radiusAlong = std::max(0.1f, fromFixed(front.radiusAlongTimes1000));
    const float radiusAcross = std::max(0.1f, fromFixed(front.radiusAcrossTimes1000));
    const float normalizedDistance = std::sqrt(
        (along * along) / (radiusAlong * radiusAlong)
        + (across * across) / (radiusAcross * radiusAcross));

    const float boundaryNoise = valueNoise(
        front.shapeSeed,
        static_cast<float>(cellX),
        static_cast<float>(cellY),
        config.getWeatherShapeNoiseCellSpan());
    const float noiseAmplitude = static_cast<float>(config.getWeatherShapeNoiseAmplitudePercent()) / 100.0f;
    const float effectiveBoundary = 1.0f + ((boundaryNoise - 0.5f) * noiseAmplitude);
    const float edgeDistance = effectiveBoundary - normalizedDistance;
    if (edgeDistance <= 0.0f) {
        return 0.0f;
    }

    const float edgeSoftness = std::max(0.05f, static_cast<float>(config.getWeatherEdgeSoftnessPercent()) / 100.0f);
    const float edgeFade = std::clamp(edgeDistance / edgeSoftness, 0.0f, 1.0f);
    if (edgeFade <= 0.0f) {
        return 0.0f;
    }

    const float alphaBase = static_cast<float>(config.getWeatherAlphaBasePercent()) / 100.0f;
    const float alphaMin = static_cast<float>(config.getWeatherAlphaMinPercent()) / 100.0f;
    const float alphaMax = static_cast<float>(config.getWeatherAlphaMaxPercent()) / 100.0f;
    const float densityMultiplier = sampleLogNormalCell(front.densitySeed, cellX, cellY, config);
    const float localAlpha = std::clamp(alphaBase * densityMultiplier, alphaMin, alphaMax);
    return std::clamp(localAlpha * edgeFade, 0.0f, 1.0f);
}

std::uint8_t encodedAlpha(float alpha) {
    return static_cast<std::uint8_t>(std::clamp(
        static_cast<int>(std::lround(alpha * 255.0f)),
        0,
        255));
}

WeatherFrontDescriptor makeStaticFrontDescriptor(WeatherDirection direction,
                                                 const sf::Vector2f& center,
                                                 float radiusAlong,
                                                 float radiusAcross,
                                                 std::uint32_t shapeSeed,
                                                 std::uint32_t densitySeed) {
    WeatherFrontDescriptor descriptor;
    descriptor.direction = direction;
    descriptor.currentTurnStep = 0;
    descriptor.totalTurnSteps = 1;
    descriptor.centerStartXTimes1000 = toFixed(center.x);
    descriptor.centerStartYTimes1000 = toFixed(center.y);
    descriptor.stepXTimes1000 = 0;
    descriptor.stepYTimes1000 = 0;
    descriptor.radiusAlongTimes1000 = toFixed(radiusAlong);
    descriptor.radiusAcrossTimes1000 = toFixed(radiusAcross);
    descriptor.shapeSeed = shapeSeed;
    descriptor.densitySeed = densitySeed;
    return descriptor;
}

bool frontHasAnyRenderedCells(const WeatherFrontDescriptor& front,
                              const Board& board,
                              const std::vector<sf::Vector2i>& validCells,
                              const GameConfig& config) {
    for (const sf::Vector2i& cellPos : validCells) {
        if (encodedAlpha(concealmentAlpha(front, board, cellPos.x, cellPos.y, config)) > 0) {
            return true;
        }
    }

    return false;
}

float maxSearchDistance(const Board& board,
                        float radiusAlong,
                        float radiusAcross,
                        float startDistance) {
    const float maxRadius = std::max(radiusAlong, radiusAcross);
    return startDistance
        + static_cast<float>(board.getDiameter())
        + ((maxRadius + 4.0f) * 6.0f);
}

float findNearestHiddenDistance(const Board& board,
                                const std::vector<sf::Vector2i>& validCells,
                                const sf::Vector2f& boundaryCenter,
                                const sf::Vector2f& normalizedDirection,
                                WeatherDirection direction,
                                float radiusAlong,
                                float radiusAcross,
                                std::uint32_t shapeSeed,
                                std::uint32_t densitySeed,
                                const GameConfig& config,
                                float startDistance,
                                float directionSign) {
    const auto hasRenderedCellsAtDistance = [&](float distanceAlongRay) {
        const sf::Vector2f center = boundaryCenter + (normalizedDirection * (distanceAlongRay * directionSign));
        const WeatherFrontDescriptor descriptor = makeStaticFrontDescriptor(
            direction,
            center,
            radiusAlong,
            radiusAcross,
            shapeSeed,
            densitySeed);
        return frontHasAnyRenderedCells(descriptor, board, validCells, config);
    };

    float low = std::max(0.0f, startDistance);
    if (!hasRenderedCellsAtDistance(low)) {
        return low;
    }

    float step = std::max(0.5f, weatherDistancePerStep(config));
    float high = low + step;
    const float searchLimit = maxSearchDistance(board, radiusAlong, radiusAcross, startDistance);
    while (high < searchLimit && hasRenderedCellsAtDistance(high)) {
        low = high;
        step *= 2.0f;
        high = std::min(searchLimit, low + step);
    }

    if (hasRenderedCellsAtDistance(high)) {
        return high;
    }

    for (int iteration = 0; iteration < 18; ++iteration) {
        const float mid = (low + high) * 0.5f;
        if (hasRenderedCellsAtDistance(mid)) {
            low = mid;
        } else {
            high = mid;
        }
    }

    return high;
}

std::uint8_t concealmentShade(std::uint8_t alpha,
                              std::uint32_t seed,
                              int cellX,
                              int cellY) {
    const float toneNoise = hashUnitFloat(seed, cellX, cellY, 97u);
    const float baseShade = 210.0f + ((toneNoise - 0.5f) * 24.0f);
    const float densityDarkening = static_cast<float>(alpha) * 0.16f;
    return static_cast<std::uint8_t>(std::clamp(
        static_cast<int>(std::lround(baseShade - densityDarkening)),
        160,
        225));
}

}

void WeatherSystem::initialize(WeatherSystemState& state,
                               std::uint32_t worldSeed,
                               int currentTurnStep,
                               const GameConfig& config) {
    state = WeatherSystemState{};
    scheduleNextSpawn(state, worldSeed, currentTurnStep, config);
    if (state.revision == 0) {
        state.revision = 1;
    }
}

bool WeatherSystem::hasActiveFront(const WeatherSystemState& state) {
    return state.hasActiveFront;
}

void WeatherSystem::clearActiveFront(WeatherSystemState& state) {
    state.hasActiveFront = false;
    state.activeFront = WeatherFrontDescriptor{};
    bumpRevision(state);
}

void WeatherSystem::scheduleNextSpawn(WeatherSystemState& state,
                                      std::uint32_t worldSeed,
                                      int currentTurnStep,
                                      const GameConfig& config) {
    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    const int delayTurns = static_cast<int>(sampleGammaTurns(
        generator,
        config.getWeatherCooldownMinTurns(),
        config.getWeatherArrivalGammaShapeTimes100(),
        config.getWeatherArrivalGammaScaleTimes100()));
    state.nextSpawnTurnStep = currentTurnStep + std::max(1, delayTurns * kStepsPerTurn);
    bumpRevision(state);
}

bool WeatherSystem::trySpawnFront(WeatherSystemState& state,
                                  const Board& board,
                                  std::uint32_t worldSeed,
                                  int currentTurnStep,
                                  const GameConfig& config) {
    if (state.hasActiveFront || currentTurnStep < state.nextSpawnTurnStep) {
        return false;
    }

    std::mt19937 generator = makeEventGenerator(state, worldSeed);
    const WeatherDirection direction = sampleDirection(state, worldSeed, config);
    const EntryEdge entryEdge = entryEdgeForDirection(direction, generator);
    const float edgePosition = sampleEdgePosition(generator, board.getDiameter(), config);
    const sf::Vector2f boundaryCenter = boundaryCenterForEdge(entryEdge, edgePosition, board.getDiameter());
    const std::array<int, 2> rawDirection = directionStep(direction);
    const sf::Vector2f normalizedDirection = normalizeDirection(rawDirection);

    const std::vector<sf::Vector2i> validCells = board.getAllValidCells();
    const int visibleCells = static_cast<int>(validCells.size());
    const int coveragePercent = std::uniform_int_distribution<int>(
        config.getWeatherCoverageMinPercent(),
        config.getWeatherCoverageMaxPercent())(generator);
    const float targetArea = std::max(1.0f, (static_cast<float>(visibleCells) * static_cast<float>(coveragePercent)) / 100.0f);
    const float aspectRatio = static_cast<float>(std::uniform_int_distribution<int>(
        config.getWeatherAspectRatioMinTimes100(),
        config.getWeatherAspectRatioMaxTimes100())(generator)) / 100.0f;
    const float radiusAlong = std::sqrt(targetArea / (kPi * std::max(1.0f, aspectRatio)));
    const float radiusAcross = std::sqrt((targetArea * std::max(1.0f, aspectRatio)) / kPi);
    const std::uint32_t shapeSeed = generator();
    const std::uint32_t densitySeed = generator();

    const float entryHiddenDistance = findNearestHiddenDistance(
        board,
        validCells,
        boundaryCenter,
        normalizedDirection,
        direction,
        radiusAlong,
        radiusAcross,
        shapeSeed,
        densitySeed,
        config,
        0.0f,
        -1.0f);
    const float boardCrossDistance = exitDistanceForRay(boundaryCenter, normalizedDirection, board.getDiameter());
    const float exitHiddenDistance = findNearestHiddenDistance(
        board,
        validCells,
        boundaryCenter,
        normalizedDirection,
        direction,
        radiusAlong,
        radiusAcross,
        shapeSeed,
        densitySeed,
        config,
        boardCrossDistance,
        1.0f);
    const float totalTravelDistance = entryHiddenDistance + exitHiddenDistance;

    const float distancePerStep = weatherDistancePerStep(config);
    const int totalTurnSteps = std::max(
        2,
        static_cast<int>(std::ceil(totalTravelDistance / distancePerStep)));

    state.hasActiveFront = true;
    state.activeFront.direction = direction;
    state.activeFront.currentTurnStep = 0;
    state.activeFront.totalTurnSteps = totalTurnSteps;
    state.activeFront.centerStartXTimes1000 = toFixed(boundaryCenter.x - (normalizedDirection.x * entryHiddenDistance));
    state.activeFront.centerStartYTimes1000 = toFixed(boundaryCenter.y - (normalizedDirection.y * entryHiddenDistance));
    state.activeFront.stepXTimes1000 = toFixed(normalizedDirection.x * distancePerStep);
    state.activeFront.stepYTimes1000 = toFixed(normalizedDirection.y * distancePerStep);
    state.activeFront.radiusAlongTimes1000 = toFixed(radiusAlong);
    state.activeFront.radiusAcrossTimes1000 = toFixed(radiusAcross);
    state.activeFront.shapeSeed = shapeSeed;
    state.activeFront.densitySeed = densitySeed;
    bumpRevision(state);
    return true;
}

bool WeatherSystem::advanceFront(WeatherSystemState& state,
                                 std::uint32_t worldSeed,
                                 int currentTurnStep,
                                 const GameConfig& config) {
    if (!state.hasActiveFront) {
        return false;
    }

    ++state.activeFront.currentTurnStep;
    if (state.activeFront.currentTurnStep >= state.activeFront.totalTurnSteps) {
        state.hasActiveFront = false;
        state.activeFront = WeatherFrontDescriptor{};
        scheduleNextSpawn(state, worldSeed, currentTurnStep, config);
        bumpRevision(state);
        return true;
    }

    bumpRevision(state);
    return true;
}

void WeatherSystem::rebuildMask(const Board& board,
                                const WeatherSystemState& state,
                                const GameConfig& config,
                                WeatherMaskCache& cache) {
    if (cache.revision == state.revision && cache.diameter == board.getDiameter()) {
        return;
    }

    const int diameter = board.getDiameter();
    cache.revision = state.revision;
    cache.diameter = diameter;
    cache.hasActiveFront = state.hasActiveFront;
    cache.alphaByCell.assign(static_cast<std::size_t>(diameter * diameter), 0);
    cache.shadeByCell.assign(static_cast<std::size_t>(diameter * diameter), 0);

    if (!state.hasActiveFront) {
        return;
    }

    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle) {
                continue;
            }

            const float alpha = concealmentAlpha(state.activeFront, board, x, y, config);
            const std::uint8_t encoded = encodedAlpha(alpha);
            if (encoded == 0) {
                continue;
            }

            const std::size_t index = static_cast<std::size_t>((y * diameter) + x);
            cache.alphaByCell[index] = encoded;
            cache.shadeByCell[index] = concealmentShade(
                cache.alphaByCell[index],
                state.activeFront.shapeSeed,
                x,
                y);
        }
    }
}

std::uint8_t WeatherSystem::alphaAtCell(const WeatherMaskCache& cache, int x, int y) {
    if (cache.diameter <= 0 || x < 0 || y < 0 || x >= cache.diameter || y >= cache.diameter) {
        return 0;
    }

    const std::size_t index = static_cast<std::size_t>((y * cache.diameter) + x);
    if (index >= cache.alphaByCell.size()) {
        return 0;
    }

    return cache.alphaByCell[index];
}

std::uint8_t WeatherSystem::shadeAtCell(const WeatherMaskCache& cache, int x, int y) {
    if (cache.diameter <= 0 || x < 0 || y < 0 || x >= cache.diameter || y >= cache.diameter) {
        return 0;
    }

    const std::size_t index = static_cast<std::size_t>((y * cache.diameter) + x);
    if (index >= cache.shadeByCell.size()) {
        return 0;
    }

    return cache.shadeByCell[index];
}

std::array<int, 2> WeatherSystem::directionStep(WeatherDirection direction) {
    switch (direction) {
        case WeatherDirection::North:
            return {0, -1};
        case WeatherDirection::South:
            return {0, 1};
        case WeatherDirection::East:
            return {1, 0};
        case WeatherDirection::West:
            return {-1, 0};
        case WeatherDirection::NorthEast:
            return {1, -1};
        case WeatherDirection::NorthWest:
            return {-1, -1};
        case WeatherDirection::SouthEast:
            return {1, 1};
        case WeatherDirection::SouthWest:
            return {-1, 1};
        case WeatherDirection::Count:
            break;
    }

    return {1, 0};
}