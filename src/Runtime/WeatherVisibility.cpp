#include "Runtime/WeatherVisibility.hpp"

#include "Autonomous/AutonomousUnit.hpp"
#include "Buildings/Building.hpp"
#include "Systems/WeatherSystem.hpp"
#include "Units/Piece.hpp"

namespace {

constexpr std::uint8_t kConcealingFogAlphaThreshold = 40;

}

bool WeatherVisibility::cellHasConcealingFog(const WeatherMaskCache& cache, sf::Vector2i cellPos) {
    return WeatherSystem::alphaAtCell(cache, cellPos.x, cellPos.y) >= kConcealingFogAlphaThreshold;
}

bool WeatherVisibility::isOccludableBuilding(const Building& building, KingdomId localPerspective) {
    return !building.isPublic() && building.owner != localPerspective;
}

bool WeatherVisibility::isOccludablePiece(const Piece& piece, KingdomId localPerspective) {
    return piece.kingdom != localPerspective;
}

bool WeatherVisibility::isOccludableAutonomousUnit(const AutonomousUnit& unit) {
    (void) unit;
    return true;
}

bool WeatherVisibility::shouldHidePiece(const Piece& piece,
                                        KingdomId localPerspective,
                                        const WeatherMaskCache& cache) {
    return isOccludablePiece(piece, localPerspective)
        && cellHasConcealingFog(cache, piece.position);
}

bool WeatherVisibility::shouldHideBuildingSelection(const Building& building,
                                                    sf::Vector2i selectionCell,
                                                    KingdomId localPerspective,
                                                    const WeatherMaskCache& cache) {
    return isOccludableBuilding(building, localPerspective)
        && cellHasConcealingFog(cache, selectionCell);
}

bool WeatherVisibility::shouldHideAutonomousUnit(const AutonomousUnit& unit,
                                                 const WeatherMaskCache& cache) {
    return isOccludableAutonomousUnit(unit)
        && cellHasConcealingFog(cache, unit.position);
}