#include "Systems/ProductionSpawnRules.hpp"

namespace {

void appendIfInBounds(std::vector<sf::Vector2i>& candidates,
                      int boardDiameter,
                      int x,
                      int y) {
    if (x < 0 || y < 0 || x >= boardDiameter || y >= boardDiameter) {
        return;
    }

    candidates.push_back({x, y});
}

} // namespace

int ProductionSpawnRules::squareColorParity(const sf::Vector2i& position) {
    return (position.x + position.y) & 1;
}

std::vector<sf::Vector2i> ProductionSpawnRules::buildSpawnCandidateOrder(const sf::Vector2i& origin,
                                                                         int footprintWidth,
                                                                         int footprintHeight,
                                                                         int boardDiameter) {
    std::vector<sf::Vector2i> candidates;
    if (boardDiameter <= 0 || footprintWidth <= 0 || footprintHeight <= 0) {
        return candidates;
    }

    candidates.reserve(boardDiameter * 8);
    for (int radius = 1; radius <= boardDiameter; ++radius) {
        const int left = origin.x - radius;
        const int right = origin.x + footprintWidth - 1 + radius;
        const int top = origin.y - radius;
        const int bottom = origin.y + footprintHeight - 1 + radius;

        for (int x = left; x <= right; ++x) {
            appendIfInBounds(candidates, boardDiameter, x, top);
        }

        for (int y = top + 1; y <= bottom - 1; ++y) {
            appendIfInBounds(candidates, boardDiameter, left, y);
            appendIfInBounds(candidates, boardDiameter, right, y);
        }

        if (bottom != top) {
            for (int x = left; x <= right; ++x) {
                appendIfInBounds(candidates, boardDiameter, x, bottom);
            }
        }
    }

    return candidates;
}

sf::Vector2i ProductionSpawnRules::findSpawnCell(const sf::Vector2i& origin,
                                                 int footprintWidth,
                                                 int footprintHeight,
                                                 int boardDiameter,
                                                 const SpawnCellValidator& isValidCell,
                                                 std::optional<int> preferredParity) {
    const std::vector<sf::Vector2i> candidates = buildSpawnCandidateOrder(
        origin, footprintWidth, footprintHeight, boardDiameter);

    if (preferredParity.has_value()) {
        for (const sf::Vector2i& candidate : candidates) {
            if (!isValidCell(candidate)) {
                continue;
            }

            if (squareColorParity(candidate) == *preferredParity) {
                return candidate;
            }
        }
    }

    for (const sf::Vector2i& candidate : candidates) {
        if (isValidCell(candidate)) {
            return candidate;
        }
    }

    return {-1, -1};
}