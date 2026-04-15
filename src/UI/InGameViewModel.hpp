#pragma once

#include <array>
#include <string>
#include <vector>

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
    std::array<KingdomBalanceMetric, 4> balanceMetrics{};
};