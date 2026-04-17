#pragma once

#include <algorithm>
#include <cstdint>
#include <SFML/System/Vector2.hpp>

#include <vector>

#include "Projection/GameSnapshot.hpp"
#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Buildings/StructurePlacementProfile.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/BuildReachRules.hpp"
#include "Systems/PendingTurnProjection.hpp"
#include "Systems/TurnPointRules.hpp"
#include "Systems/TurnCommand.hpp"
#include "Systems/TurnValidationContext.hpp"

namespace BuildOverlayRules {

struct BuildOverlayMap {
    std::vector<sf::Vector2i> validOrigins;
    std::vector<sf::Vector2i> validAnchorCells;
    std::vector<sf::Vector2i> coverageCells;
};

namespace detail {

inline int goldBuildCost(BuildingType type, const GameConfig& config) {
    switch (type) {
        case BuildingType::Barracks:
            return config.getBarracksCost();
        case BuildingType::WoodWall:
            return config.getWoodWallCost();
        case BuildingType::StoneWall:
            return config.getStoneWallCost();
        case BuildingType::Arena:
            return config.getArenaCost();
        default:
            return -1;
    }
}

inline int cellIndex(int x, int y, int width) {
    return y * width + x;
}

inline int prefixIndex(int x, int y, int stride) {
    return y * stride + x;
}

inline void markPieceCells(const std::vector<SnapPiece>& pieces,
                           std::vector<std::uint8_t>& blockedCells,
                           int diameter) {
    for (const SnapPiece& piece : pieces) {
        if (piece.position.x < 0 || piece.position.y < 0
            || piece.position.x >= diameter || piece.position.y >= diameter) {
            continue;
        }

        blockedCells[cellIndex(piece.position.x, piece.position.y, diameter)] = 1;
    }
}

inline void markBuildingCells(const std::vector<SnapBuilding>& buildings,
                              std::vector<std::uint8_t>& blockedCells,
                              int diameter) {
    for (const SnapBuilding& building : buildings) {
        for (const sf::Vector2i& cell : building.getOccupiedCells()) {
            if (cell.x < 0 || cell.y < 0 || cell.x >= diameter || cell.y >= diameter) {
                continue;
            }

            blockedCells[cellIndex(cell.x, cell.y, diameter)] = 1;
        }
    }
}

inline std::vector<int> buildBlockedPrefix(const GameSnapshot& snapshot) {
    const int diameter = snapshot.getDiameter();
    std::vector<std::uint8_t> blockedCells(static_cast<std::size_t>(diameter * diameter), 0);
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            if (!snapshot.isTraversable(x, y)) {
                blockedCells[cellIndex(x, y, diameter)] = 1;
            }
        }
    }

    markPieceCells(snapshot.white.pieces, blockedCells, diameter);
    markPieceCells(snapshot.black.pieces, blockedCells, diameter);
    markBuildingCells(snapshot.white.buildings, blockedCells, diameter);
    markBuildingCells(snapshot.black.buildings, blockedCells, diameter);
    markBuildingCells(snapshot.publicBuildings, blockedCells, diameter);

    const int stride = diameter + 1;
    std::vector<int> prefix(static_cast<std::size_t>(stride * stride), 0);
    for (int y = 0; y < diameter; ++y) {
        int rowSum = 0;
        for (int x = 0; x < diameter; ++x) {
            rowSum += blockedCells[cellIndex(x, y, diameter)];
            prefix[prefixIndex(x + 1, y + 1, stride)] = prefix[prefixIndex(x + 1, y, stride)] + rowSum;
        }
    }

    return prefix;
}

inline int blockedRectangleCount(const std::vector<int>& prefix,
                                 int diameter,
                                 int originX,
                                 int originY,
                                 int width,
                                 int height) {
    const int stride = diameter + 1;
    const int x0 = originX;
    const int y0 = originY;
    const int x1 = originX + width;
    const int y1 = originY + height;
    return prefix[prefixIndex(x1, y1, stride)]
        - prefix[prefixIndex(x0, y1, stride)]
        - prefix[prefixIndex(x1, y0, stride)]
        + prefix[prefixIndex(x0, y0, stride)];
}

} // namespace detail

inline BuildOverlayMap collectBuildOverlayMap(const TurnValidationContext& context,
                                              const std::vector<TurnCommand>& pendingCommands,
                                              BuildingType buildingType,
                                              int rotationQuarterTurns) {
    BuildOverlayMap map;

    const PendingTurnProjectionResult projection = PendingTurnProjection::project(
        context,
        pendingCommands);
    if (!projection.valid) {
        return map;
    }

    const GameSnapshot& snapshot = projection.snapshot;
    const SnapKingdom& projectedKingdom = snapshot.kingdom(context.activeKingdom.id);
    const SnapTurnBudget& projectedBudget = snapshot.turnBudget(context.activeKingdom.id);
    const int buildPointCost = TurnPointRules::buildCost(buildingType, context.config);
    const int buildGoldCost = detail::goldBuildCost(buildingType, context.config);
    if (buildPointCost <= 0 || buildGoldCost < 0) {
        return map;
    }

    if (projectedBudget.buildPointsRemaining < buildPointCost || projectedKingdom.gold < buildGoldCost) {
        return map;
    }

    const int baseWidth = context.config.getBuildingWidth(buildingType);
    const int baseHeight = context.config.getBuildingHeight(buildingType);
    const int footprintWidth = Building::getFootprintWidthFor(baseWidth, baseHeight, rotationQuarterTurns);
    const int footprintHeight = Building::getFootprintHeightFor(baseWidth, baseHeight, rotationQuarterTurns);
    const int diameter = snapshot.getDiameter();
    if (footprintWidth <= 0 || footprintHeight <= 0 || diameter <= 0) {
        return map;
    }

    const std::vector<sf::Vector2i> builderPositions = collectBuilderPositions(projectedKingdom.pieces);
    if (builderPositions.empty()) {
        return map;
    }

    const std::vector<int> blockedPrefix = detail::buildBlockedPrefix(snapshot);
    std::vector<std::uint8_t> visitedOrigins(static_cast<std::size_t>(diameter * diameter), 0);
    std::vector<std::uint8_t> coverageMask(static_cast<std::size_t>(diameter * diameter), 0);
    map.validOrigins.reserve(builderPositions.size() * static_cast<std::size_t>(footprintWidth + 2) * static_cast<std::size_t>(footprintHeight + 2));
    map.validAnchorCells.reserve(map.validOrigins.capacity());

    for (const sf::Vector2i& builderPos : builderPositions) {
        const int minOriginX = std::max(0, builderPos.x - footprintWidth);
        const int maxOriginX = std::min(diameter - 1, builderPos.x + 1);
        const int minOriginY = std::max(0, builderPos.y - footprintHeight);
        const int maxOriginY = std::min(diameter - 1, builderPos.y + 1);

        for (int originY = minOriginY; originY <= maxOriginY; ++originY) {
            if (originY + footprintHeight > diameter) {
                continue;
            }

            for (int originX = minOriginX; originX <= maxOriginX; ++originX) {
                if (originX + footprintWidth > diameter) {
                    continue;
                }

                const int originIndex = detail::cellIndex(originX, originY, diameter);
                if (visitedOrigins[originIndex]) {
                    continue;
                }
                visitedOrigins[originIndex] = 1;

                if (detail::blockedRectangleCount(
                        blockedPrefix,
                        diameter,
                        originX,
                        originY,
                        footprintWidth,
                        footprintHeight) != 0) {
                    continue;
                }

                map.validOrigins.push_back({originX, originY});
                map.validAnchorCells.push_back(StructurePlacementProfiles::anchorCellFromOrigin(
                    buildingType,
                    {originX, originY},
                    baseWidth,
                    baseHeight,
                    rotationQuarterTurns));
                for (int localY = 0; localY < footprintHeight; ++localY) {
                    for (int localX = 0; localX < footprintWidth; ++localX) {
                        coverageMask[detail::cellIndex(originX + localX, originY + localY, diameter)] = 1;
                    }
                }
            }
        }
    }

    map.coverageCells.reserve(map.validOrigins.size() * static_cast<std::size_t>(footprintWidth * footprintHeight));
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            if (coverageMask[detail::cellIndex(x, y, diameter)]) {
                map.coverageCells.push_back({x, y});
            }
        }
    }

    return map;
}

inline BuildOverlayMap collectBuildOverlayMap(const Board& board,
                                              const Kingdom& activeKingdom,
                                              const Kingdom& enemyKingdom,
                                              const std::vector<Building>& publicBuildings,
                                              int turnNumber,
                                              const std::vector<TurnCommand>& pendingCommands,
                                              BuildingType buildingType,
                                              int rotationQuarterTurns,
                                              const GameConfig& config) {
    return collectBuildOverlayMap(
        TurnValidationContext{board, activeKingdom, enemyKingdom, publicBuildings, turnNumber, config},
        pendingCommands,
        buildingType,
        rotationQuarterTurns);
}

inline std::vector<sf::Vector2i> collectBuildableOrigins(const Board& board,
                                                         const Kingdom& activeKingdom,
                                                         const Kingdom& enemyKingdom,
                                                         const std::vector<Building>& publicBuildings,
                                                         int turnNumber,
                                                         const std::vector<TurnCommand>& pendingCommands,
                                                         BuildingType buildingType,
                                                         int rotationQuarterTurns,
                                                         const GameConfig& config) {
    return collectBuildOverlayMap(
        board,
        activeKingdom,
        enemyKingdom,
        publicBuildings,
        turnNumber,
        pendingCommands,
        buildingType,
        rotationQuarterTurns,
        config).validOrigins;
}

inline std::vector<sf::Vector2i> collectBuildableCoverageCells(const Board& board,
                                                               const Kingdom& activeKingdom,
                                                               const Kingdom& enemyKingdom,
                                                               const std::vector<Building>& publicBuildings,
                                                               int turnNumber,
                                                               const std::vector<TurnCommand>& pendingCommands,
                                                               BuildingType buildingType,
                                                               int rotationQuarterTurns,
                                                               const GameConfig& config) {
    return collectBuildOverlayMap(
        board,
        activeKingdom,
        enemyKingdom,
        publicBuildings,
        turnNumber,
        pendingCommands,
        buildingType,
        rotationQuarterTurns,
        config).coverageCells;
}

} // namespace BuildOverlayRules