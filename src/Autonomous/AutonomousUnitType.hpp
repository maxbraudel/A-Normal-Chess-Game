#pragma once

enum class AutonomousUnitType {
    InfernalPiece = 0
};

inline constexpr const char* autonomousUnitTypeDisplayName(AutonomousUnitType type) {
    switch (type) {
        case AutonomousUnitType::InfernalPiece:
            return "Infernal Piece";
    }

    return "Unknown";
}