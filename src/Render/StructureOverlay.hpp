#pragma once

#include <string>
#include <vector>

#include "Kingdom/KingdomId.hpp"
#include "Units/PieceType.hpp"

class Board;
class Building;
class GameConfig;

enum class PublicBuildingOccupationState {
    Unoccupied,
    WhiteOccupied,
    BlackOccupied,
    Contested
};

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
StructureOverlayStack buildSelectedStructureOverlay(const Building& building,
                                                    const Board& board,
                                                    const GameConfig& config);