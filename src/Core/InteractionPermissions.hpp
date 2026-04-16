#pragma once

#include "Core/GameState.hpp"

struct InteractionPermissionInputs {
    GameState gameState = GameState::MainMenu;
    bool overlaysVisible = false;
    bool inGameMenuOpen = false;
    bool waitingForRemoteTurnResult = false;
    bool multiplayerSessionReady = true;
    bool isLocalPlayerTurn = false;
    bool activeKingInCheck = false;
    bool projectedKingInCheck = false;
    bool hasAnyLegalResponse = true;
};

struct InteractionPermissions {
    bool canOpenMenu = false;
    bool canMoveCamera = false;
    bool canInspectWorld = false;
    bool canUseToolbar = false;
    bool canOpenBuildPanel = false;
    bool canIssueCommands = false;
    bool canQueueNonMoveActions = false;
    bool canShowActionOverlays = false;
    bool canShowBuildPreview = false;
};

InteractionPermissions computeInteractionPermissions(const InteractionPermissionInputs& inputs);