#pragma once

#include <SFML/System/Vector2.hpp>

#include <functional>
#include <optional>
#include <vector>

class ProductionSpawnRules {
public:
    using SpawnCellValidator = std::function<bool(const sf::Vector2i&)>;

    static int squareColorParity(const sf::Vector2i& position);
    static std::vector<sf::Vector2i> buildSpawnCandidateOrder(const sf::Vector2i& anchorCell,
                                                              int boardDiameter);
    static sf::Vector2i findSpawnCell(const sf::Vector2i& anchorCell,
                                      int boardDiameter,
                                      const SpawnCellValidator& isValidCell,
                                      std::optional<int> preferredParity = std::nullopt);
};