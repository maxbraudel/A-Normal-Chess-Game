#pragma once

#include <SFML/Graphics/Color.hpp>

struct DamagedStructuresRenderingConfig {
    int opacityPercent = 70;
};

struct SelectionOverlayRenderingConfig {
    sf::Color selectionFrame{80, 160, 255, 240};
    sf::Color originCellSafe{40, 120, 255, 130};
    sf::Color reachableCell{0, 255, 0, 80};
    sf::Color dangerCell{255, 40, 40, 90};
};

struct BuildPreviewRenderingConfig {
    sf::Color validFill{36, 196, 84, 88};
    sf::Color invalidFill{214, 52, 52, 96};
    sf::Color validOutline{92, 255, 144, 196};
    sf::Color invalidOutline{255, 120, 120, 208};
};

struct StructureOverlayRenderingConfig {
    sf::Color background{24, 24, 28, 220};
    sf::Color outline{235, 235, 235, 210};
    sf::Color fill{80, 160, 255, 220};
    sf::Color text{248, 248, 248, 240};
    int iconSizePx = 28;
    int actionMarkerSizePx = 24;
    int progressWidthPx = 96;
    int progressHeightPx = 18;
    int itemGapPx = 6;
    int rowGapPx = 4;
    int marginAboveBuildingPx = 8;
    int textSizePx = 13;
};

struct OrientationCheckerboardRenderingConfig {
    sf::Color dark{36, 36, 36, 96};
    sf::Color light{255, 255, 255, 48};
};

struct QueuedMovePathRenderingConfig {
    sf::Color color{255, 255, 255, 255};
    int layerOpacityPercent = 50;
    int thicknessPx = 10;
    int dottedDotSizePx = 5;
    int dottedGapPx = 4;
};

struct TacticalGridRenderingConfig {
    sf::Color checkerDark{156, 156, 156, 255};
    sf::Color checkerLight{236, 236, 236, 255};
    sf::Color blockedTerrain{86, 112, 146, 255};
    sf::Color blockedStructure{54, 54, 54, 255};
};

struct RenderingStyleConfig {
    DamagedStructuresRenderingConfig damagedStructures;
    SelectionOverlayRenderingConfig selectionOverlay;
    BuildPreviewRenderingConfig buildPreview;
    StructureOverlayRenderingConfig structureOverlay;
    OrientationCheckerboardRenderingConfig orientationCheckerboard;
    QueuedMovePathRenderingConfig queuedMovePaths;
    TacticalGridRenderingConfig tacticalGrid;
};