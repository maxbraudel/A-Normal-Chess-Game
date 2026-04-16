#include "Render/StructureOverlay.hpp"

#include <algorithm>

#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/PublicBuildingOccupation.hpp"
#include "Units/Piece.hpp"

namespace {

StructureOverlayIcon makeUITextureIcon(const std::string& textureName) {
    StructureOverlayIcon icon;
    icon.source = StructureOverlayIconSource::UITexture;
    icon.textureName = textureName;
    return icon;
}

StructureOverlayIcon makePieceIcon(PieceType type, KingdomId kingdom) {
    StructureOverlayIcon icon;
    icon.source = StructureOverlayIconSource::PieceTexture;
    icon.pieceType = type;
    icon.kingdom = kingdom;
    return icon;
}

StructureOverlayItem makeIconItem(const StructureOverlayIcon& icon) {
    StructureOverlayItem item;
    item.type = StructureOverlayItemType::Icon;
    item.icon = icon;
    return item;
}

StructureOverlayItem makeProgressBarItem(float progress, const std::string& label) {
    StructureOverlayItem item;
    item.type = StructureOverlayItemType::ProgressBar;
    item.progress = progress;
    item.text = label;
    return item;
}

StructureOverlayIcon makeOwnerShieldIcon(KingdomId owner) {
    return makeUITextureIcon(owner == KingdomId::White ? "shield_white" : "shield_black");
}

} // namespace

bool StructureOverlayStack::isEmpty() const {
    return rows.empty();
}

StructureOverlayVisibility StructureOverlayPolicy::visibilityFor(StructureOverlayIndicatorKind kind) const {
    switch (kind) {
        case StructureOverlayIndicatorKind::Occupation:
            return occupationVisibility;
        case StructureOverlayIndicatorKind::Construction:
            return constructionVisibility;
        case StructureOverlayIndicatorKind::BarracksProduction:
            return barracksProductionVisibility;
    }

    return StructureOverlayVisibility::Never;
}

bool StructureOverlayPolicy::shouldShow(StructureOverlayIndicatorKind kind,
                                        const StructureOverlayContext& context) const {
    switch (visibilityFor(kind)) {
        case StructureOverlayVisibility::Always:
            return true;
        case StructureOverlayVisibility::WhenSelected:
            return context.isSelected;
        case StructureOverlayVisibility::Never:
            return false;
    }

    return false;
}

float computeBarracksProductionProgress(const Building& barracks, const GameConfig& config) {
    if (barracks.type != BuildingType::Barracks || !barracks.isProducing) {
        return 0.f;
    }

    const PieceType pieceType = static_cast<PieceType>(barracks.producingType);
    const int totalTurns = std::max(0, config.getProductionTurns(pieceType));
    if (totalTurns <= 0) {
        return 1.f;
    }

    const int remainingTurns = std::max(0, barracks.turnsRemaining);
    const int completedTurns = std::clamp(totalTurns - remainingTurns, 0, totalTurns);
    return static_cast<float>(completedTurns) / static_cast<float>(totalTurns);
}

std::string formatTurnsRemainingLabel(int turnsRemaining) {
    const int clampedTurns = std::max(0, turnsRemaining);
    return std::to_string(clampedTurns) + (clampedTurns == 1 ? " turn" : " turns");
}

StructureOverlayPolicy makeWorldStructureOverlayPolicy() {
    StructureOverlayPolicy policy;
    policy.occupationVisibility = StructureOverlayVisibility::Always;
    policy.constructionVisibility = StructureOverlayVisibility::Always;
    policy.barracksProductionVisibility = StructureOverlayVisibility::Always;
    return policy;
}

StructureOverlayStack buildStructureOverlay(const Building& building,
                                            const Board& board,
                                            const GameConfig& config,
                                            const StructureOverlayContext& context,
                                            const StructureOverlayPolicy& policy) {
    StructureOverlayStack stack;

    StructureOverlayRow statusRow;
    statusRow.placement = StructureOverlayRowPlacement::Above;
    if (policy.shouldShow(StructureOverlayIndicatorKind::Occupation, context)) {
        if (building.isPublic()) {
            switch (resolvePublicBuildingOccupationState(building, board)) {
                case PublicBuildingOccupationState::WhiteOccupied:
                    statusRow.items.push_back(makeIconItem(makeUITextureIcon("shield_white")));
                    break;
                case PublicBuildingOccupationState::BlackOccupied:
                    statusRow.items.push_back(makeIconItem(makeUITextureIcon("shield_black")));
                    break;
                case PublicBuildingOccupationState::Contested:
                    statusRow.items.push_back(makeIconItem(makeUITextureIcon("crossed_swords")));
                    break;
                case PublicBuildingOccupationState::Unoccupied:
                    break;
            }
        } else {
            statusRow.items.push_back(makeIconItem(makeOwnerShieldIcon(building.owner)));
        }
    }

    if (building.isUnderConstruction()
        && policy.shouldShow(StructureOverlayIndicatorKind::Construction, context)) {
        statusRow.items.push_back(makeIconItem(makeUITextureIcon("build_ongoing")));
    }

    if (!statusRow.items.empty()) {
        stack.rows.push_back(std::move(statusRow));
    }

    if (building.type == BuildingType::Barracks
        && building.isProducing
        && policy.shouldShow(StructureOverlayIndicatorKind::BarracksProduction, context)) {
        StructureOverlayRow productionRow;
        productionRow.placement = StructureOverlayRowPlacement::Below;
        productionRow.items.push_back(makeIconItem(
            makePieceIcon(static_cast<PieceType>(building.producingType), building.owner)));
        productionRow.items.push_back(makeProgressBarItem(
            computeBarracksProductionProgress(building, config),
            formatTurnsRemainingLabel(building.turnsRemaining)));
        stack.rows.push_back(std::move(productionRow));
    }

    return stack;
}