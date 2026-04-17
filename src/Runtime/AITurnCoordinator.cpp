#include "Runtime/AITurnCoordinator.hpp"

#include <iostream>

#include "AI/AIDirector.hpp"
#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"

namespace {

const char* pieceTypeName(PieceType type) {
    switch (type) {
        case PieceType::Pawn: return "Pawn";
        case PieceType::Knight: return "Knight";
        case PieceType::Bishop: return "Bishop";
        case PieceType::Rook: return "Rook";
        case PieceType::Queen: return "Queen";
        case PieceType::King: return "King";
    }

    return "Unknown";
}

const char* buildingTypeName(BuildingType type) {
    switch (type) {
        case BuildingType::Church: return "Church";
        case BuildingType::Mine: return "Mine";
        case BuildingType::Farm: return "Farm";
        case BuildingType::Barracks: return "Barracks";
        case BuildingType::WoodWall: return "WoodWall";
        case BuildingType::StoneWall: return "StoneWall";
        case BuildingType::Bridge: return "Bridge";
        case BuildingType::Arena: return "Arena";
    }

    return "Unknown";
}

const char* turnCommandName(TurnCommand::Type type) {
    switch (type) {
        case TurnCommand::Move: return "Move";
        case TurnCommand::Build: return "Build";
        case TurnCommand::Produce: return "Produce";
        case TurnCommand::Upgrade: return "Upgrade";
        case TurnCommand::Marry: return "Marry";
        case TurnCommand::FormGroup: return "FormGroup";
        case TurnCommand::BreakGroup: return "BreakGroup";
        case TurnCommand::Disband: return "Disband";
    }

    return "Unknown";
}

} // namespace

AITurnCoordinator::AITurnCoordinator(GameEngine& engine,
                                     AITurnRunner& turnRunner,
                                     AIDirector& director,
                                     const GameConfig& config)
    : m_engine(engine)
    , m_turnRunner(turnRunner)
    , m_director(director)
    , m_config(config) {}

AITurnStartPlan AITurnCoordinator::buildStartPlan(const AITurnRuntimeState& state) {
    AITurnStartPlan plan;
    if (state.gameState != GameState::Playing || !state.activeAI || state.runnerBusy) {
        return plan;
    }

    plan.shouldStart = true;
    plan.activeKingdom = state.activeKingdom;
    plan.turnNumber = state.turnNumber;
    return plan;
}

AITurnCompletionPlan AITurnCoordinator::buildCompletionPlan(
    const AITurnRuntimeState& state,
    const std::optional<AITurnRunner::CompletedTurn>& completedTurn) {
    AITurnCompletionPlan plan;
    if (!completedTurn.has_value()) {
        return plan;
    }

    if (state.gameState != GameState::Playing
        || state.activeKingdom != completedTurn->activeKingdom
        || state.turnNumber != completedTurn->turnNumber) {
        plan.shouldIgnore = true;
        return plan;
    }

    plan.applyPlanMetadata = true;
    plan.printDebugSummary = true;
    plan.stageTurn = true;
    plan.logObjective = true;
    plan.logPlanningComplete = true;
    plan.commitAuthoritativeTurn = true;
    plan.activeKingdom = completedTurn->activeKingdom;
    plan.turnNumber = completedTurn->turnNumber;
    plan.objectiveName = completedTurn->plan.objectiveName;
    plan.debugLines = buildDebugLines(completedTurn->plan);
    return plan;
}

void AITurnCoordinator::cancelPendingTurn() {
    m_turnRunner.cancel();
}

void AITurnCoordinator::startTurnIfNeeded(GameState gameState) {
    const AITurnStartPlan plan = buildStartPlan(makeRuntimeState(gameState));
    if (!plan.shouldStart) {
        return;
    }

    m_turnRunner.start(m_engine.board(),
                       m_engine.kingdoms(),
                       m_engine.publicBuildings(),
                       m_engine.mapObjects(),
                       m_engine.autonomousUnits(),
                       plan.activeKingdom,
                       plan.turnNumber,
                       m_engine.sessionConfig().worldSeed,
                       m_engine.xpSystemState(),
                       m_config,
                       m_director);
}

bool AITurnCoordinator::pollCompletedTurn(GameState gameState) {
    const std::optional<AITurnRunner::CompletedTurn> completedTurn = m_turnRunner.poll();
    const AITurnCompletionPlan plan = buildCompletionPlan(makeRuntimeState(gameState), completedTurn);
    if (!completedTurn.has_value() || plan.shouldIgnore) {
        return false;
    }

    const AIDirectorPlan& directorPlan = completedTurn->plan;
    if (plan.applyPlanMetadata) {
        m_director.applyPlanMetadata(directorPlan);
    }
    if (plan.printDebugSummary) {
        std::cout << "AI PLAN | objective=" << plan.objectiveName
                  << " | commands=" << directorPlan.commands.size() << '\n';
        for (const std::string& line : plan.debugLines) {
            std::cout << line << '\n';
        }
    }
    if (plan.logObjective) {
        m_engine.eventLog().log(plan.turnNumber,
                                plan.activeKingdom,
                                "AI Objective: " + plan.objectiveName);
    }
    if (plan.stageTurn) {
        m_engine.stageAITurnPlan(directorPlan.commands, m_config);
    }
    if (plan.logPlanningComplete) {
        m_engine.eventLog().log(plan.turnNumber,
                                plan.activeKingdom,
                                "AI completed turn planning.");
    }

    return plan.commitAuthoritativeTurn;
}

AITurnRuntimeState AITurnCoordinator::makeRuntimeState(GameState gameState) const {
    AITurnRuntimeState state;
    state.gameState = gameState;
    state.activeAI = m_engine.isActiveAI();
    state.runnerBusy = m_turnRunner.isRunning();
    state.activeKingdom = m_engine.turnSystem().getActiveKingdom();
    state.turnNumber = m_engine.turnSystem().getTurnNumber();
    return state;
}

std::vector<std::string> AITurnCoordinator::buildDebugLines(const AIDirectorPlan& plan) {
    std::vector<std::string> lines;
    lines.reserve(plan.commands.size());
    for (const TurnCommand& command : plan.commands) {
        std::string line = std::string{"  - "} + turnCommandName(command.type);
        if (command.type == TurnCommand::Move) {
            line += " piece=" + std::to_string(command.pieceId)
                + " from=(" + std::to_string(command.origin.x) + "," + std::to_string(command.origin.y) + ")"
                + " to=(" + std::to_string(command.destination.x) + "," + std::to_string(command.destination.y) + ")";
        } else if (command.type == TurnCommand::Build) {
            line += " building=" + std::string{buildingTypeName(command.buildingType)}
                + " origin=(" + std::to_string(command.buildOrigin.x) + "," + std::to_string(command.buildOrigin.y) + ")";
        } else if (command.type == TurnCommand::Produce) {
            line += " barracks=" + std::to_string(command.barracksId)
                + " unit=" + std::string{pieceTypeName(command.produceType)};
        }
        lines.push_back(std::move(line));
    }

    return lines;
}