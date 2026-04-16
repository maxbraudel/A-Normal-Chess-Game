#pragma once

#include <cstdint>
#include <vector>

#include <SFML/System/Vector2.hpp>

#include "Buildings/BuildingType.hpp"
#include "Core/InteractionPermissions.hpp"
#include "Core/ToolState.hpp"
#include "Kingdom/KingdomId.hpp"

struct TurnCommand;
struct TurnValidationContext;

struct BuildOverlayCache {
    std::vector<sf::Vector2i> validAnchorCells;
    std::vector<sf::Vector2i> coverageCells;
    std::uint64_t revision = 0;
    int turnNumber = -1;
    KingdomId activeKingdom = KingdomId::White;
    BuildingType buildType = BuildingType::Barracks;
    int rotationQuarterTurns = 0;
    bool canQueueNonMoveActions = false;
    bool cacheValid = false;
};

struct BuildOverlayRuntimeState {
    ToolState activeTool = ToolState::Select;
    InteractionPermissions permissions{};
    std::uint64_t revision = 0;
    int turnNumber = -1;
    KingdomId activeKingdom = KingdomId::White;
    BuildingType buildType = BuildingType::Barracks;
    int rotationQuarterTurns = 0;
    bool canQueueNonMoveActions = false;
};

class BuildOverlayCoordinator {
public:
    static void invalidate(BuildOverlayCache& cache);
    static void refresh(const BuildOverlayRuntimeState& runtimeState,
                        const TurnValidationContext& turnContext,
                        const std::vector<TurnCommand>& pendingCommands,
                        BuildOverlayCache& cache);
};