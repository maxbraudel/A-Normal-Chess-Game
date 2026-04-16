#pragma once
#include <SFML/System/Vector2.hpp>
#include "Units/PieceType.hpp"
#include "Buildings/BuildingType.hpp"

struct TurnCommand {
    enum Type { Move, Build, Produce, Upgrade, Marry, FormGroup, BreakGroup };
    Type type;

    // Move
    int pieceId = -1;
    sf::Vector2i origin{0, 0};       // where the piece was before the move
    sf::Vector2i destination{0, 0};

    // Build
    int buildId = -1;
    BuildingType buildingType = BuildingType::Barracks;
    sf::Vector2i buildOrigin{0, 0};
    int buildRotationQuarterTurns = 0;

    // Produce
    int barracksId = -1;
    PieceType produceType = PieceType::Pawn;

    // Upgrade
    int upgradePieceId = -1;
    PieceType upgradeTarget = PieceType::Knight;

    // Formation
    int formationId = -1;
};
