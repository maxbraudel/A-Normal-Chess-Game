#include "AI/AIController.hpp"
#include "AI/AIEvaluator.hpp"
#include "AI/AIStrategyEcon.hpp"
#include "AI/AIStrategyBuild.hpp"
#include "AI/AIStrategyMove.hpp"
#include "AI/AIStrategySpecial.hpp"
#include "Board/Board.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/TurnSystem.hpp"
#include "Systems/TurnCommand.hpp"
#include "Systems/EventLog.hpp"
#include "Config/GameConfig.hpp"
#include "Buildings/Building.hpp"
#include <cstdlib>
#include <ctime>

AIController::AIController() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

bool AIController::loadConfig(const std::string& filepath) {
    return m_config.loadFromFile(filepath);
}

void AIController::playTurn(Board& board, Kingdom& self, Kingdom& enemy,
                              const std::vector<Building>& publicBuildings,
                              TurnSystem& turnSystem, const GameConfig& config, EventLog& log) {
    // Evaluate current position
    float score = AIEvaluator::evaluate(board, self, enemy, config);
    (void)score; // Used for strategic decisions if needed

    bool hasMoved = false;
    bool hasBuilt = false;
    bool hasProduced = false;
    bool hasMarried = false;

    // Apply randomness: small chance to skip a strategy
    auto shouldSkip = [&]() -> bool {
        if (m_config.randomness <= 0.0f) return false;
        float roll = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        return roll < m_config.randomness;
    };

    // 1. Special actions first (upgrades, marriage)
    if (!shouldSkip()) {
        auto specialCmds = AIStrategySpecial::decide(board, self, enemy, publicBuildings,
                                                       config, m_config, hasMarried);
        for (auto& cmd : specialCmds) {
            if (turnSystem.queueCommand(cmd)) {
                if (cmd.type == TurnCommand::Marry) hasMarried = true;
            }
        }
    }

    // 2. Economic decisions (move to farms, build barracks, produce)
    if (!shouldSkip()) {
        auto econCmds = AIStrategyEcon::decide(board, self, enemy, config, m_config,
                                                 hasMoved, hasBuilt, hasProduced);
        for (auto& cmd : econCmds) {
            if (turnSystem.queueCommand(cmd)) {
                if (cmd.type == TurnCommand::Move) hasMoved = true;
                if (cmd.type == TurnCommand::Build) hasBuilt = true;
                if (cmd.type == TurnCommand::Produce) hasProduced = true;
            }
        }
    }

    // 3. Build decisions (walls, arena)
    if (!hasBuilt && !shouldSkip()) {
        auto buildCmds = AIStrategyBuild::decide(board, self, enemy, config, m_config, hasBuilt);
        for (auto& cmd : buildCmds) {
            if (turnSystem.queueCommand(cmd)) {
                hasBuilt = true;
            }
        }
    }

    // 4. Movement/attack decisions
    if (!hasMoved && !shouldSkip()) {
        auto moveCmds = AIStrategyMove::decide(board, self, enemy, config, m_config, hasMoved);
        for (auto& cmd : moveCmds) {
            if (turnSystem.queueCommand(cmd)) {
                hasMoved = true;
            }
        }
    }

    // Log AI turn
    log.log(turnSystem.getTurnNumber(), self.id, "AI completed turn planning.");
}
