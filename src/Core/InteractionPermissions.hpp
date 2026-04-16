#pragma once

#include <optional>

#include <SFML/System/Vector2.hpp>

#include "Buildings/BuildingType.hpp"
#include "Core/GameState.hpp"
#include "Input/ToolState.hpp"
#include "Kingdom/KingdomId.hpp"

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

struct InputSelectionBookmark {
    ToolState tool = ToolState::Select;
    int pieceId = -1;
    int buildingId = -1;
    std::optional<sf::Vector2i> selectedCell;
    std::optional<sf::Vector2i> selectedBuildingOrigin;
    BuildingType selectedBuildingType = BuildingType::Barracks;
    KingdomId selectedBuildingOwner = KingdomId::White;
    bool selectedBuildingIsNeutral = false;
    int selectedBuildingRotationQuarterTurns = 0;
    BuildingType buildPreviewType = BuildingType::Barracks;
    int buildPreviewRotationQuarterTurns = 0;
    std::optional<sf::Vector2i> buildPreviewAnchorCell;
};

InteractionPermissions computeInteractionPermissions(const InteractionPermissionInputs& inputs);