#include "UI/InGameViewModelBuilder.hpp"

#include <algorithm>

#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/CheckSystem.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/MarriageSystem.hpp"
#include "Systems/PendingTurnProjection.hpp"

namespace {

std::string actorLabelForEvent(const GameEngine& engine, KingdomId kingdomId) {
    return engine.participantName(kingdomId) + " - " + kingdomLabel(kingdomId);
}

std::string pieceTypeName(PieceType type) {
    switch (type) {
        case PieceType::Pawn: return "Pawn";
        case PieceType::Knight: return "Knight";
        case PieceType::Bishop: return "Bishop";
        case PieceType::Rook: return "Rook";
        case PieceType::Queen: return "Queen";
        case PieceType::King: return "King";
    }

    return "Piece";
}

std::string buildingTypeName(BuildingType type) {
    switch (type) {
        case BuildingType::Church: return "Church";
        case BuildingType::Mine: return "Mine";
        case BuildingType::Farm: return "Farm";
        case BuildingType::Barracks: return "Barracks";
        case BuildingType::WoodWall: return "Wood Wall";
        case BuildingType::StoneWall: return "Stone Wall";
        case BuildingType::Bridge: return "Bridge";
        case BuildingType::Arena: return "Arena";
    }

    return "Building";
}

std::string cellLabel(sf::Vector2i cell) {
    return "(" + std::to_string(cell.x) + ", " + std::to_string(cell.y) + ")";
}

void appendPlannedActionRows(InGameViewModel& model,
                             const GameEngine& engine,
                             const GameConfig& config) {
    const Kingdom& activeKingdom = engine.activeKingdom();
    for (const TurnCommand& command : engine.turnSystem().getPendingCommands()) {
        InGamePlannedActionRow row;
        row.kindLabel = "Queued";

        switch (command.type) {
            case TurnCommand::Move: {
                const Piece* piece = activeKingdom.getPieceById(command.pieceId);
                row.actionLabel = "Move " + (piece ? pieceTypeName(piece->type) : std::string("Piece"));
                row.detailLabel = cellLabel(command.origin) + " -> " + cellLabel(command.destination);
                break;
            }
            case TurnCommand::Build:
                row.actionLabel = "Build " + buildingTypeName(command.buildingType);
                row.detailLabel = "At " + cellLabel(command.buildOrigin);
                break;
            case TurnCommand::Produce:
                row.actionLabel = "Recruit " + pieceTypeName(command.produceType);
                row.detailLabel = "Barracks #" + std::to_string(command.barracksId);
                break;
            case TurnCommand::Upgrade: {
                const Piece* piece = activeKingdom.getPieceById(command.upgradePieceId);
                const std::string currentType = piece ? pieceTypeName(piece->type) : std::string("Piece");
                row.actionLabel = "Upgrade " + currentType;
                row.detailLabel = currentType + " -> " + pieceTypeName(command.upgradeTarget);
                break;
            }
            case TurnCommand::Disband: {
                const Piece* piece = activeKingdom.getPieceById(command.pieceId);
                row.actionLabel = "Tuer la piece";
                row.detailLabel = piece ? pieceTypeName(piece->type) : std::string("Piece");
                break;
            }
            case TurnCommand::Marry:
                row.actionLabel = "Church coronation";
                row.detailLabel = "Manual command";
                break;
            case TurnCommand::FormGroup:
                row.actionLabel = "Form group";
                row.detailLabel = "Formation #" + std::to_string(command.formationId);
                break;
            case TurnCommand::BreakGroup:
                row.actionLabel = "Break group";
                row.detailLabel = "Formation #" + std::to_string(command.formationId);
                break;
        }

        model.plannedActionRows.push_back(std::move(row));
    }

    const PendingTurnProjectionResult projection = PendingTurnProjection::project(
        engine.board(),
        engine.activeKingdom(),
        engine.enemyKingdom(),
        engine.publicBuildings(),
        engine.turnSystem().getTurnNumber(),
        engine.turnSystem().getPendingCommands(),
        config);
    if (projection.valid
        && MarriageSystem::canPerformChurchCoronation(projection.snapshot, activeKingdom.id)) {
        model.plannedActionRows.push_back({
            "Auto",
            "Church coronation",
            "A rook becomes Queen at validation",
            true
        });
    }
}

} // namespace

int countOccupiedBuildingCells(const Kingdom& kingdom) {
    int occupiedCells = 0;
    for (const auto& building : kingdom.buildings) {
        if (building.isDestroyed()) {
            continue;
        }

        occupiedCells += static_cast<int>(building.getOccupiedCells().size());
    }

    return occupiedCells;
}

InGameViewModel buildInGameViewModel(const GameEngine& engine,
                                     const GameConfig& config,
                                     GameState state,
                                     bool allowCommands,
                                     const InGameHudPresentation& presentation) {
    InGameViewModel model;
    const Kingdom& activeKingdom = engine.activeKingdom();
    const Kingdom& displayedHudKingdom = engine.kingdom(presentation.statsKingdom);
    const Kingdom& whiteKingdom = engine.kingdom(KingdomId::White);
    const Kingdom& blackKingdom = engine.kingdom(KingdomId::Black);

    model.turnNumber = engine.turnSystem().getTurnNumber();
    model.activeTurnLabel = engine.activeTurnLabel();
    model.showTurnPointIndicators = presentation.showTurnPointIndicators;
    model.turnIndicatorTone = presentation.turnIndicatorTone;
    if (state == GameState::GameOver) {
        model.alerts.push_back({"Checkmate", InGameAlertTone::Danger});
    } else if (CheckSystem::isInCheck(engine.activeKingdom().id, engine.board(), config)) {
        model.alerts.push_back({"Check", InGameAlertTone::Danger});
    }
    model.activeGold = displayedHudKingdom.gold;
    model.activeOccupiedCells = countOccupiedBuildingCells(displayedHudKingdom);
    model.activeTroops = displayedHudKingdom.pieceCount();
    model.activeIncome = EconomySystem::calculateProjectedNetIncome(displayedHudKingdom,
                                                                    engine.board(),
                                                                    engine.publicBuildings(),
                                                                    config);
    model.activeMovementPointsAvailable = engine.turnSystem().getMovementPointsRemaining();
    model.activeMovementPointsTotal = engine.turnSystem().getMovementPointsMax();
    model.activeBuildPointsAvailable = engine.turnSystem().getBuildPointsRemaining();
    model.activeBuildPointsTotal = engine.turnSystem().getBuildPointsMax();
    model.allowCommands = allowCommands;
    model.canEndTurn = allowCommands;
    appendPlannedActionRows(model, engine, config);

    const auto& events = engine.eventLog().getEvents();
    const std::size_t startIndex = (events.size() > 60) ? (events.size() - 60) : 0;
    for (std::size_t index = startIndex; index < events.size(); ++index) {
        const auto& event = events[index];
        model.eventRows.push_back({
            event.turnNumber,
            actorLabelForEvent(engine, event.kingdom),
            event.message
        });
    }

    model.balanceMetrics = {{
        {inGameMetricLabels()[0], whiteKingdom.gold, blackKingdom.gold},
        {inGameMetricLabels()[1], countOccupiedBuildingCells(whiteKingdom), countOccupiedBuildingCells(blackKingdom)},
        {inGameMetricLabels()[2], whiteKingdom.pieceCount(), blackKingdom.pieceCount()},
        {inGameMetricLabels()[3],
         EconomySystem::calculateProjectedNetIncome(whiteKingdom, engine.board(), engine.publicBuildings(), config),
         EconomySystem::calculateProjectedNetIncome(blackKingdom, engine.board(), engine.publicBuildings(), config)}
    }};

    return model;
}