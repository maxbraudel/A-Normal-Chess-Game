#include "Runtime/BuildOverlayCoordinator.hpp"

#include "Systems/BuildOverlayRules.hpp"

namespace {

bool matchesCacheKey(const BuildOverlayCache& cache, const BuildOverlayRuntimeState& runtimeState) {
    return cache.cacheValid
        && cache.revision == runtimeState.revision
        && cache.turnNumber == runtimeState.turnNumber
        && cache.activeKingdom == runtimeState.activeKingdom
        && cache.buildType == runtimeState.buildType
    && cache.rotationQuarterTurns == runtimeState.rotationQuarterTurns
    && cache.canQueueNonMoveActions == runtimeState.canQueueNonMoveActions;
}

} // namespace

void BuildOverlayCoordinator::invalidate(BuildOverlayCache& cache) {
    cache.validAnchorCells.clear();
    cache.coverageCells.clear();
    cache.cacheValid = false;
}

void BuildOverlayCoordinator::refresh(const BuildOverlayRuntimeState& runtimeState,
                                     const TurnValidationContext& turnContext,
                                     const std::vector<TurnCommand>& pendingCommands,
                                     BuildOverlayCache& cache) {
    if (runtimeState.activeTool != ToolState::Build || !runtimeState.permissions.canShowBuildPreview) {
        invalidate(cache);
        return;
    }

    if (matchesCacheKey(cache, runtimeState)) {
        return;
    }

    if (!runtimeState.permissions.canQueueNonMoveActions) {
        cache.validAnchorCells.clear();
        cache.coverageCells.clear();
    } else {
        const BuildOverlayRules::BuildOverlayMap buildOverlayMap = BuildOverlayRules::collectBuildOverlayMap(
            turnContext,
            pendingCommands,
            runtimeState.buildType,
            runtimeState.rotationQuarterTurns);
        cache.validAnchorCells = std::move(buildOverlayMap.validAnchorCells);
        cache.coverageCells = std::move(buildOverlayMap.coverageCells);
    }

    cache.revision = runtimeState.revision;
    cache.turnNumber = runtimeState.turnNumber;
    cache.activeKingdom = runtimeState.activeKingdom;
    cache.buildType = runtimeState.buildType;
    cache.rotationQuarterTurns = runtimeState.rotationQuarterTurns;
    cache.canQueueNonMoveActions = runtimeState.canQueueNonMoveActions;
    cache.cacheValid = true;
}