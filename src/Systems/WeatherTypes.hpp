#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

enum class WeatherDirection {
    North = 0,
    South,
    East,
    West,
    NorthEast,
    NorthWest,
    SouthEast,
    SouthWest,
    Count
};

constexpr std::size_t kNumWeatherDirections = static_cast<std::size_t>(WeatherDirection::Count);

constexpr std::size_t weatherDirectionIndex(WeatherDirection direction) {
    return static_cast<std::size_t>(direction);
}

struct WeatherFrontDescriptor {
    WeatherDirection direction = WeatherDirection::East;
    int currentTurnStep = 0;
    int totalTurnSteps = 0;
    int centerStartXTimes1000 = 0;
    int centerStartYTimes1000 = 0;
    int stepXTimes1000 = 0;
    int stepYTimes1000 = 0;
    int radiusAlongTimes1000 = 0;
    int radiusAcrossTimes1000 = 0;
    std::uint32_t shapeSeed = 0;
    std::uint32_t densitySeed = 0;
};

struct WeatherSystemState {
    int nextSpawnTurnStep = 0;
    bool hasActiveFront = false;
    WeatherFrontDescriptor activeFront{};
    std::uint32_t rngCounter = 0;
    std::uint32_t revision = 0;
};

struct WeatherMaskCache {
    std::uint32_t revision = 0;
    int diameter = 0;
    bool hasActiveFront = false;
    std::vector<std::uint8_t> alphaByCell;
    std::vector<std::uint8_t> shadeByCell;

    void clear() {
        revision = 0;
        diameter = 0;
        hasActiveFront = false;
        alphaByCell.clear();
        shadeByCell.clear();
    }
};