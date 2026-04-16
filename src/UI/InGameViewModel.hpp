#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "Kingdom/KingdomId.hpp"

inline constexpr std::size_t kInGameMetricCount = 4;

inline const std::array<std::string, kInGameMetricCount>& inGameMetricLabels() {
    static const std::array<std::string, kInGameMetricCount> labels = {
        "Gold",
        "Occupied Cells",
        "Troops",
        "Net Income"
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

struct InGamePlannedActionRow {
    std::string kindLabel;
    std::string actionLabel;
    std::string detailLabel;
    bool predicted = false;
};

enum class InGameTurnIndicatorTone {
    Neutral,
    LocalTurn
};

struct InGameHudPresentation {
    KingdomId statsKingdom = KingdomId::White;
    bool showTurnPointIndicators = true;
    InGameTurnIndicatorTone turnIndicatorTone = InGameTurnIndicatorTone::Neutral;
};

enum class InGameAlertTone {
    Neutral,
    Warning,
    Danger,
    Success
};

struct InGameAlert {
    std::string text;
    InGameAlertTone tone = InGameAlertTone::Neutral;
};

struct InGameViewModel {
    int turnNumber = 1;
    std::string activeTurnLabel;
    int activeGold = 0;
    int activeOccupiedCells = 0;
    int activeTroops = 0;
    int activeIncome = 0;
    int activeMovementPointsAvailable = 0;
    int activeMovementPointsTotal = 0;
    int activeBuildPointsAvailable = 0;
    int activeBuildPointsTotal = 0;
    bool allowCommands = false;
    bool canEndTurn = false;
    bool showTurnPointIndicators = true;
    InGameTurnIndicatorTone turnIndicatorTone = InGameTurnIndicatorTone::Neutral;
    std::vector<InGameAlert> alerts;
    std::vector<InGameEventRow> eventRows;
    std::vector<InGamePlannedActionRow> plannedActionRows;
    std::array<KingdomBalanceMetric, kInGameMetricCount> balanceMetrics{};
};