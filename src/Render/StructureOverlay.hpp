#pragma once

#include <string>
#include <vector>

#include "Kingdom/KingdomId.hpp"
#include "Systems/PublicBuildingOccupation.hpp"
#include "Units/PieceType.hpp"

class Board;
class Building;
class GameConfig;

enum class StructureOverlayIconSource {
    UITexture,
    PieceTexture
};

struct StructureOverlayIcon {
    StructureOverlayIconSource source = StructureOverlayIconSource::UITexture;
    std::string textureName;
    PieceType pieceType = PieceType::Pawn;
    KingdomId kingdom = KingdomId::White;
};

enum class StructureOverlayItemType {
    Icon,
    Text,
    ProgressBar
};

enum class StructureOverlayIndicatorKind {
    Occupation,
    Construction,
    BarracksProduction
};

enum class StructureOverlayVisibility {
    Always,
    WhenSelected,
    Never
};

struct StructureOverlayContext {
    bool isSelected = false;
};

struct StructureOverlayPolicy {
    StructureOverlayVisibility occupationVisibility = StructureOverlayVisibility::Always;
    StructureOverlayVisibility constructionVisibility = StructureOverlayVisibility::Always;
    StructureOverlayVisibility barracksProductionVisibility = StructureOverlayVisibility::Always;

    StructureOverlayVisibility visibilityFor(StructureOverlayIndicatorKind kind) const;
    bool shouldShow(StructureOverlayIndicatorKind kind, const StructureOverlayContext& context) const;
};

struct StructureOverlayItem {
    StructureOverlayItemType type = StructureOverlayItemType::Icon;
    StructureOverlayIcon icon;
    std::string text;
    float progress = 0.f;
};

enum class StructureOverlayRowPlacement {
    Above,
    Below
};

struct StructureOverlayRow {
    StructureOverlayRowPlacement placement = StructureOverlayRowPlacement::Above;
    std::vector<StructureOverlayItem> items;
};

struct StructureOverlayStack {
    std::vector<StructureOverlayRow> rows;

    bool isEmpty() const;
};

PublicBuildingOccupationState resolvePublicBuildingOccupationState(const Building& building,
                                                                   const Board& board);
float computeBarracksProductionProgress(const Building& barracks, const GameConfig& config);
std::string formatTurnsRemainingLabel(int turnsRemaining);
StructureOverlayPolicy makeWorldStructureOverlayPolicy();
StructureOverlayStack buildStructureOverlay(const Building& building,
                                            const Board& board,
                                            const GameConfig& config,
                                            const StructureOverlayContext& context,
                                            const StructureOverlayPolicy& policy);