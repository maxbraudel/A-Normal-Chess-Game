#pragma once

#include <algorithm>
#include <cmath>
#include <random>

#include "Systems/XPTypes.hpp"

namespace RewardProfileSampling {

inline int sampleTruncatedNormal(const XPRewardProfile& profile, std::mt19937& generator) {
    const int minimum = std::max(0, profile.minimum);
    if (profile.mean <= 0) {
        return minimum;
    }

    const double mean = static_cast<double>(profile.mean);
    const double sigmaMultiplier = static_cast<double>(profile.sigmaMultiplierTimes100) / 100.0;
    const double clampMultiplier = static_cast<double>(profile.clampSigmaMultiplierTimes100) / 100.0;
    if (sigmaMultiplier <= 0.0 || clampMultiplier <= 0.0) {
        return std::max(minimum, profile.mean);
    }

    const double sigma = std::max(1.0, mean * sigmaMultiplier);
    const double delta = sigma * clampMultiplier;
    const double minValue = std::max(static_cast<double>(minimum), mean - delta);
    const double maxValue = std::max(minValue, mean + delta);

    std::normal_distribution<double> distribution(mean, sigma);
    const double clampedSample = std::clamp(distribution(generator), minValue, maxValue);
    return std::max(minimum, static_cast<int>(std::lround(clampedSample)));
}

} // namespace RewardProfileSampling