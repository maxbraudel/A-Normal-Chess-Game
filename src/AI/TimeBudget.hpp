#pragma once
#include <chrono>
#include <algorithm>

/// Monotonic time-budget guard — ensures AI never freezes the game.
/// Every sub-system checks `expired()` before doing expensive work.
class TimeBudget {
public:
    explicit TimeBudget(int budgetMs)
        : m_start(std::chrono::steady_clock::now())
        , m_budgetMs(budgetMs) {}

    int elapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start).count());
    }

    int remainingMs() const { return std::max(0, m_budgetMs - elapsedMs()); }
    bool expired() const { return elapsedMs() >= m_budgetMs; }
    bool hasAtLeast(int ms) const { return remainingMs() >= ms; }

private:
    std::chrono::steady_clock::time_point m_start;
    int m_budgetMs;
};
