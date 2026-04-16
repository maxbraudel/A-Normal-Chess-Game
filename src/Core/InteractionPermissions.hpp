#pragma once

#include <optional>

#include <SFML/System/Vector2.hpp>

#include "Core/GameState.hpp"
#include "Input/PendingBuildSelection.hpp"
#include "Input/ToolState.hpp"

struct InteractionPermissionInputs {
    GameState gameState = GameState::MainMenu;
    bool overlaysVisible = false;
    bool inGameMenuOpen = false;
    bool waitingForRemoteTurnResult = false;
    bool multiplayerSessionReady = true;
    bool isLocalPlayerTurn = false;
    bool activeKingInCheck = false;
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
};

struct InputSelectionBookmark {
    ToolState tool = ToolState::Select;
    int pieceId = -1;
    int buildingId = -1;
    std::optional<sf::Vector2i> selectedCell;
    std::optional<PendingBuildSelection> pendingBuildSelection;
};

InteractionPermissions computeInteractionPermissions(const InteractionPermissionInputs& inputs);