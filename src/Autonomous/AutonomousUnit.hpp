#pragma once

#include <string>

#include <SFML/System/Vector2.hpp>

#include "Autonomous/AutonomousUnitType.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Units/PieceType.hpp"

enum class InfernalPhase {
    Hunting = 0,
    Returning = 1
};

struct InfernalUnitData {
    KingdomId targetKingdom = KingdomId::White;
    int targetPieceId = -1;
    PieceType manifestedPieceType = PieceType::Queen;
    PieceType preferredTargetType = PieceType::Queen;
    InfernalPhase phase = InfernalPhase::Hunting;
    sf::Vector2i returnBorderCell{-1, -1};
    int spawnTurn = 0;
};

struct AutonomousUnit {
    int id = -1;
    AutonomousUnitType type = AutonomousUnitType::InfernalPiece;
    sf::Vector2i position{0, 0};
    InfernalUnitData infernal{};
};

inline constexpr const char* infernalPhaseDisplayName(InfernalPhase phase) {
    switch (phase) {
        case InfernalPhase::Hunting:
            return "Hunting";
        case InfernalPhase::Returning:
            return "Returning";
    }

    return "Unknown";
}

inline bool isValidInfernalReturnBorderCell(sf::Vector2i borderCell) {
    return borderCell.x >= 0 && borderCell.y >= 0;
}

inline std::string describeInfernalTarget(const AutonomousUnit& unit) {
    return std::string(autonomousUnitTypeDisplayName(unit.type))
        + " targeting "
        + ((unit.infernal.targetKingdom == KingdomId::White) ? "White" : "Black");
}