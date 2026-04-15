#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

inline constexpr std::size_t kInGameMetricCount = 4;

inline const std::array<std::string, kInGameMetricCount>& inGameMetricLabels() {
    static const std::array<std::string, kInGameMetricCount> labels = {
        "Gold",
        "Occupied Cells",
        "Troops",
        "Income"
    };

    return labels;
}

struct InGameEventRow {
    int turnNumber = 0;
    std::string actorLabel;
    std::string actionLabel;
};

struct KingdomBalanceMetric {
    std::string label;
    int whiteValue = 0;
    int blackValue = 0;
};

struct InGameViewModel {
    int turnNumber = 1;
    std::string activeTurnLabel;
    std::string statusLabel = "Idle";
    int activeGold = 0;
    int activeOccupiedCells = 0;
    int activeTroops = 0;
    int activeIncome = 0;
    bool allowCommands = false;
    std::vector<InGameEventRow> eventRows;
    std::array<KingdomBalanceMetric, kInGameMetricCount> balanceMetrics{};
};