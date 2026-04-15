#include "UI/InGameViewModelBuilder.hpp"

#include <algorithm>

#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/CheckSystem.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/EventLog.hpp"

namespace {

std::string turnStatusLabel(const GameEngine& engine,
                            const GameConfig& config,
                            GameState state) {
    if (state == GameState::GameOver) {
        return "Checkmate";
    }

    if (CheckSystem::isInCheck(engine.activeKingdom().id, engine.board(), config)) {
        return "Check";
    }

    return "Idle";
}

std::string actorLabelForEvent(const GameEngine& engine, KingdomId kingdomId) {
    return engine.participantName(kingdomId) + " - " + kingdomLabel(kingdomId);
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
                                     bool allowCommands) {
    InGameViewModel model;
    const Kingdom& activeKingdom = engine.activeKingdom();
    const Kingdom& whiteKingdom = engine.kingdom(KingdomId::White);
    const Kingdom& blackKingdom = engine.kingdom(KingdomId::Black);

    model.turnNumber = engine.turnSystem().getTurnNumber();
    model.activeTurnLabel = engine.activeTurnLabel();
    model.statusLabel = turnStatusLabel(engine, config, state);
    model.activeGold = activeKingdom.gold;
    model.activeOccupiedCells = countOccupiedBuildingCells(activeKingdom);
    model.activeTroops = activeKingdom.pieceCount();
    model.activeIncome = EconomySystem::calculateProjectedIncome(activeKingdom,
                                                                 engine.board(),
                                                                 engine.publicBuildings(),
                                                                 config);
    model.allowCommands = allowCommands;

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
        {inGameMetricLabels()[3],
         EconomySystem::calculateProjectedIncome(whiteKingdom, engine.board(), engine.publicBuildings(), config),
         EconomySystem::calculateProjectedIncome(blackKingdom, engine.board(), engine.publicBuildings(), config)},
        {inGameMetricLabels()[2], whiteKingdom.pieceCount(), blackKingdom.pieceCount()},
        {inGameMetricLabels()[1], countOccupiedBuildingCells(whiteKingdom), countOccupiedBuildingCells(blackKingdom)}
    }};

    return model;
}