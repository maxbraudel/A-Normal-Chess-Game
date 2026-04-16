#include "Core/InteractionPermissions.hpp"

InteractionPermissions computeInteractionPermissions(const InteractionPermissionInputs& inputs) {
    InteractionPermissions permissions;

    const bool interactiveWorldState = inputs.gameState == GameState::Playing
        || inputs.gameState == GameState::Paused
        || inputs.gameState == GameState::GameOver;

    permissions.canOpenMenu = interactiveWorldState && !inputs.overlaysVisible;
    permissions.canMoveCamera = interactiveWorldState
        && !inputs.overlaysVisible
        && !inputs.inGameMenuOpen;
    permissions.canInspectWorld = interactiveWorldState
        && !inputs.overlaysVisible
        && !inputs.inGameMenuOpen;
    permissions.canUseToolbar = interactiveWorldState
        && !inputs.overlaysVisible;
    permissions.canOpenBuildPanel = interactiveWorldState
        && !inputs.overlaysVisible;

    const bool checkmated = inputs.activeKingInCheck && !inputs.hasAnyLegalResponse;
    const bool canIssueCommands = inputs.gameState == GameState::Playing
        && !inputs.overlaysVisible
        && !inputs.waitingForRemoteTurnResult
        && inputs.multiplayerSessionReady
        && inputs.isLocalPlayerTurn
        && !checkmated;

    permissions.canIssueCommands = canIssueCommands;
    permissions.canQueueNonMoveActions = canIssueCommands && !inputs.projectedKingInCheck;
    permissions.canShowActionOverlays = canIssueCommands;
    permissions.canShowBuildPreview = permissions.canInspectWorld;
    return permissions;
}