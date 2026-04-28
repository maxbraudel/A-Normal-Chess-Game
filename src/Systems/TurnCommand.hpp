#pragma once
#include <SFML/System/Vector2.hpp>
#include <string>
#include "Units/PieceType.hpp"
#include "Buildings/BuildingType.hpp"

struct TurnCommand {
    enum Type { Move, Build, Produce, Upgrade, Marry, FormGroup, BreakGroup, Disband };
    Type type = Move;

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

enum class TurnCommandAuditAction {
    Queue = 0,
    Replace,
    Cancel,
    Reset
};

struct TurnCommandAuditEntry {
    int sequence = 0;
    int turnNumber = 0;
    long long turnElapsedMs = 0;
    TurnCommandAuditAction action = TurnCommandAuditAction::Queue;
    bool accepted = false;
    bool hasCommand = false;
    TurnCommand command{};
    std::string reason;
};
