#pragma once

#include <SFML/System/Vector2.hpp>

#include "Kingdom/KingdomId.hpp"
#include "Systems/WeatherTypes.hpp"

class Building;
class Piece;
struct AutonomousUnit;

class WeatherVisibility {
public:
    static bool cellHasConcealingFog(const WeatherMaskCache& cache, sf::Vector2i cellPos);
    static bool isOccludableBuilding(const Building& building, KingdomId localPerspective);
    static bool isOccludablePiece(const Piece& piece, KingdomId localPerspective);
    static bool isOccludableAutonomousUnit(const AutonomousUnit& unit);
    static bool shouldHidePiece(const Piece& piece,
                                KingdomId localPerspective,
                                const WeatherMaskCache& cache);
    static bool shouldHideBuildingSelection(const Building& building,
                                            sf::Vector2i selectionCell,
                                            KingdomId localPerspective,
                                            const WeatherMaskCache& cache);
    static bool shouldHideAutonomousUnit(const AutonomousUnit& unit,
                                         const WeatherMaskCache& cache);
};