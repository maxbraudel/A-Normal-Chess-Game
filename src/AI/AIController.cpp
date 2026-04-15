#include "AI/AIController.hpp"
#include "AI/AITurnContext.hpp"
#include "AI/AIBrain.hpp"
#include "AI/AITacticalEngine.hpp"
#include "AI/AIStrategyEcon.hpp"
#include "AI/AIStrategyBuild.hpp"
#include "AI/AIStrategyMove.hpp"
#include "AI/AIStrategySpecial.hpp"
#include "Board/Board.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/TurnSystem.hpp"
#include "Systems/TurnCommand.hpp"
#include "Systems/CheckSystem.hpp"
#include "Systems/EventLog.hpp"
#include "Config/GameConfig.hpp"
#include "Buildings/Building.hpp"
#include <iostream>

AIController::AIController() = default;

bool AIController::loadConfig(const std::string& filepath) {
    return m_config.loadFromFile(filepath);
}

const AIBrain& AIController::getBrain() const {
    return m_brain;
}

AITurnPlan AIController::computeTurnPlan(Board& board, Kingdom& self, Kingdom& enemy,
                                         const std::vector<Building>& publicBuildings,
                                         int turnNumber, const GameConfig& config) {
    AITurnPlan plan;

    // === Step 0: Build the cached turn context (computed ONCE) ===
    AITurnContext ctx;
    ctx.build(board, self, enemy, config);

    const Piece* trackedEnemyKing = enemy.getKing();
    if (trackedEnemyKing) {
        if (trackedEnemyKing->position == m_lastEnemyKingPos) {
            ++m_enemyKingStaticTurns;
        } else {
            m_lastEnemyKingPos = trackedEnemyKing->position;
            m_enemyKingStaticTurns = 0;
        }
    } else {
        m_lastEnemyKingPos = {-9999, -9999};
        m_enemyKingStaticTurns = 0;
    }

    plan.lastEnemyKingPos = m_lastEnemyKingPos;
    plan.enemyKingStaticTurns = m_enemyKingStaticTurns;

    // === Step 1: Update brain using cached threat maps ===
    m_brain.update(board, self, enemy, config, m_config, ctx,
                   turnNumber, m_enemyKingStaticTurns);
    const auto& priorities = m_brain.getPriorities();
    const bool forcePressure = (m_enemyKingStaticTurns >= 4 && m_brain.hasSufficientMatingMaterial(self));
    plan.phaseName = m_brain.getPhaseName();

    int turn = turnNumber;
    std::cerr << "\n========== AI TURN " << turn << " ==========" << std::endl;
    std::cerr << "  Phase: " << m_brain.getPhaseName() << std::endl;
    std::cerr << "  Priorities: econ=" << priorities.economy
              << " prod=" << priorities.production
              << " build=" << priorities.building
              << " atk=" << priorities.attack
              << " def=" << priorities.defense << std::endl;
    std::cerr << "  Self pieces: " << self.pieces.size() << ", Enemy pieces: " << enemy.pieces.size() << std::endl;
    for (const auto& p : self.pieces) {
        std::cerr << "    [" << p.id << "] " << static_cast<int>(p.type)
                  << " @ (" << p.position.x << "," << p.position.y << ")";
        const Cell& pc = board.getCell(p.position.x, p.position.y);
        if (pc.building) std::cerr << " ON building=" << static_cast<int>(pc.building->type);
        std::cerr << std::endl;
    }
    std::cerr << "  Gold: " << self.gold << std::endl;
    std::cerr << "  Free resource cells: " << ctx.freeResourceCells.size() << std::endl;
    std::cerr << "  Enemy king static turns: " << m_enemyKingStaticTurns
              << " forcePressure=" << forcePressure << std::endl;
    Piece* dbgKing = self.getKing();
    if (dbgKing) {
        std::cerr << "  King @ (" << dbgKing->position.x << "," << dbgKing->position.y << ")";
        std::cerr << " threatened=" << ctx.enemyThreats.isSet(dbgKing->position) << std::endl;
        auto kit = ctx.selfMoves.find(dbgKing->id);
        if (kit != ctx.selfMoves.end())
            std::cerr << "  King has " << kit->second.size() << " legal moves" << std::endl;
    }

    TurnSystem planningTurnSystem;
    planningTurnSystem.setActiveKingdom(self.id);

    auto queuePlanned = [&](const TurnCommand& cmd) {
        if (planningTurnSystem.queueCommand(cmd,
                                            board,
                                            self,
                                            enemy,
                                            publicBuildings,
                                            config)) {
            plan.commands.push_back(cmd);
            return true;
        }
        return false;
    };

    bool hasMoved = false;
    bool hasBuilt = false;
    bool hasProduced = false;
    bool hasMarried = false;

    auto shouldSkip = [&]() -> bool {
        (void)forcePressure;
        return false;
    };

    // === Step 2: CRISIS — resolve check and stop. Non-move actions are forbidden while in check. ===
    if (m_brain.getPhase() == AIPhase::CRISIS) {
        std::cerr << "  >> CRISIS branch entered" << std::endl;
        auto moveCmds = AIStrategyMove::decide(board, self, enemy, config, m_config,
                                                m_brain, m_tacticalEngine, ctx,
                                                publicBuildings, hasMoved);
        std::cerr << "  >> CRISIS move commands: " << moveCmds.size() << std::endl;
        for (auto& cmd : moveCmds) {
            if (queuePlanned(cmd)) {
                if (cmd.type == TurnCommand::Move) hasMoved = true;
            }
        }
        return plan;
    }

    // === Normal turn: Special → [Move OR Econ first, based on phase] → Build ===
    // In attack phases, Move first so the army advances.
    // In eco phases, Econ first to gather resources.

    // 1. Special actions (upgrades, marriage)
    if (!forcePressure && !shouldSkip()) {
        auto specialCmds = AIStrategySpecial::decide(board, self, enemy, publicBuildings,
                                                       config, m_config, m_brain, hasMarried);
        for (auto& cmd : specialCmds) {
            if (queuePlanned(cmd)) {
                if (cmd.type == TurnCommand::Marry) hasMarried = true;
            }
        }
    }

    bool attackPhase = (m_brain.getPhase() == AIPhase::AGGRESSION
                     || m_brain.getPhase() == AIPhase::MID_GAME
                     || m_brain.getPhase() == AIPhase::ENDGAME
                     || forcePressure);

    // 2a. In ATTACK phases: Move FIRST, then Econ
    if (attackPhase) {
        // Movement/attack
        if (!hasMoved && !shouldSkip()) {
            std::cerr << "  >> Move strategy: entering (attack-phase priority)" << std::endl;
            auto moveCmds = AIStrategyMove::decide(board, self, enemy, config, m_config,
                                                    m_brain, m_tacticalEngine, ctx,
                                                    publicBuildings, hasMoved);
            std::cerr << "  >> Move returned " << moveCmds.size() << " commands" << std::endl;
            for (auto& cmd : moveCmds) {
                std::cerr << "     Move cmd piece=" << cmd.pieceId << " from=(" << cmd.origin.x << "," << cmd.origin.y << ") to=(" << cmd.destination.x << "," << cmd.destination.y << ")" << std::endl;
                if (queuePlanned(cmd)) {
                    hasMoved = true;
                }
            }
        }
        // Then economy (if move slot not used)
        if (!forcePressure && !shouldSkip() && priorities.economy >= 0.2f) {
            std::cerr << "  >> Econ: hasMoved=" << hasMoved << " hasBuilt=" << hasBuilt << " hasProduced=" << hasProduced << std::endl;
            auto econCmds = AIStrategyEcon::decide(board, self, enemy, config, m_config,
                                                     m_brain, m_tacticalEngine, ctx,
                                                     hasMoved, hasBuilt, hasProduced);
            std::cerr << "  >> Econ returned " << econCmds.size() << " commands" << std::endl;
            for (auto& cmd : econCmds) {
                std::cerr << "     Econ cmd type=" << cmd.type;
                if (cmd.type == TurnCommand::Move) std::cerr << " piece=" << cmd.pieceId << " to=(" << cmd.destination.x << "," << cmd.destination.y << ")";
                if (cmd.type == TurnCommand::Build) std::cerr << " building=" << static_cast<int>(cmd.buildingType);
                if (cmd.type == TurnCommand::Produce) std::cerr << " unit=" << static_cast<int>(cmd.produceType);
                std::cerr << std::endl;
                if (queuePlanned(cmd)) {
                    if (cmd.type == TurnCommand::Move) hasMoved = true;
                    if (cmd.type == TurnCommand::Build) hasBuilt = true;
                    if (cmd.type == TurnCommand::Produce) hasProduced = true;
                }
            }
        }
    }
    // 2b. In ECO phases: Econ FIRST, then Move
    else {
        // Economy first
        if (!shouldSkip() && priorities.economy >= 0.2f) {
            std::cerr << "  >> Econ: hasMoved=" << hasMoved << " hasBuilt=" << hasBuilt << " hasProduced=" << hasProduced << std::endl;
            auto econCmds = AIStrategyEcon::decide(board, self, enemy, config, m_config,
                                                     m_brain, m_tacticalEngine, ctx,
                                                     hasMoved, hasBuilt, hasProduced);
            std::cerr << "  >> Econ returned " << econCmds.size() << " commands" << std::endl;
            for (auto& cmd : econCmds) {
                std::cerr << "     Econ cmd type=" << cmd.type;
                if (cmd.type == TurnCommand::Move) std::cerr << " piece=" << cmd.pieceId << " to=(" << cmd.destination.x << "," << cmd.destination.y << ")";
                if (cmd.type == TurnCommand::Build) std::cerr << " building=" << static_cast<int>(cmd.buildingType);
                if (cmd.type == TurnCommand::Produce) std::cerr << " unit=" << static_cast<int>(cmd.produceType);
                std::cerr << std::endl;
                if (queuePlanned(cmd)) {
                    if (cmd.type == TurnCommand::Move) hasMoved = true;
                    if (cmd.type == TurnCommand::Build) hasBuilt = true;
                    if (cmd.type == TurnCommand::Produce) hasProduced = true;
                }
            }
        }
        // Then movement
        if (!hasMoved && !shouldSkip()) {
            std::cerr << "  >> Move strategy: entering" << std::endl;
            auto moveCmds = AIStrategyMove::decide(board, self, enemy, config, m_config,
                                                    m_brain, m_tacticalEngine, ctx,
                                                    publicBuildings, hasMoved);
            std::cerr << "  >> Move returned " << moveCmds.size() << " commands" << std::endl;
            for (auto& cmd : moveCmds) {
                std::cerr << "     Move cmd piece=" << cmd.pieceId << " from=(" << cmd.origin.x << "," << cmd.origin.y << ") to=(" << cmd.destination.x << "," << cmd.destination.y << ")" << std::endl;
                if (queuePlanned(cmd)) {
                    hasMoved = true;
                }
            }
        }
    }

    // 3. Building decisions
    if (!forcePressure && !hasBuilt && !shouldSkip() && priorities.building >= 0.2f) {
        std::cerr << "  >> Build: entering (gold=" << self.gold << " building=" << priorities.building << ")" << std::endl;
        auto buildCmds = AIStrategyBuild::decide(board, self, enemy, config, m_config,
                                                   m_brain, hasBuilt);
        std::cerr << "  >> Build returned " << buildCmds.size() << " commands" << std::endl;
        for (auto& cmd : buildCmds) {
            std::cerr << "     Build cmd: type=" << static_cast<int>(cmd.buildingType)
                      << " at=(" << cmd.buildOrigin.x << "," << cmd.buildOrigin.y << ")" << std::endl;
            if (queuePlanned(cmd)) {
                hasBuilt = true;
            }
        }
    } else {
        std::cerr << "  >> Build SKIPPED (hasBuilt=" << hasBuilt
                  << " building=" << priorities.building << ")" << std::endl;
    }

    // 4. Production retry
    if (!hasProduced) {
        std::cerr << "  >> Production retry (gold=" << self.gold << " prod=" << priorities.production << ")" << std::endl;
        auto econCmds = AIStrategyEcon::decide(board, self, enemy, config, m_config,
                                                 m_brain, m_tacticalEngine, ctx,
                                                 true, true, hasProduced);
        for (auto& cmd : econCmds) {
            if (cmd.type == TurnCommand::Produce) {
                std::cerr << "     Produce cmd: unit=" << static_cast<int>(cmd.produceType) << std::endl;
                if (queuePlanned(cmd)) {
                    hasProduced = true;
                }
            }
        }
    }

    std::cerr << "  >> FINAL: hasMoved=" << hasMoved << " hasBuilt=" << hasBuilt
              << " hasProduced=" << hasProduced << " hasMarried=" << hasMarried << std::endl;
    std::cerr << "========== END AI TURN ==========" << std::endl;
    return plan;
}

void AIController::applyTurnPlanMetadata(const AITurnPlan& plan) {
    m_lastEnemyKingPos = plan.lastEnemyKingPos;
    m_enemyKingStaticTurns = plan.enemyKingStaticTurns;
}

void AIController::playTurn(Board& board, Kingdom& self, Kingdom& enemy,
                              const std::vector<Building>& publicBuildings,
                              TurnSystem& turnSystem, const GameConfig& config, EventLog& log) {
    AITurnPlan plan = computeTurnPlan(board, self, enemy, publicBuildings,
                                      turnSystem.getTurnNumber(), config);
    applyTurnPlanMetadata(plan);
    log.log(turnSystem.getTurnNumber(), self.id, "AI Phase: " + plan.phaseName);
    for (const auto& cmd : plan.commands) {
        turnSystem.queueCommand(cmd,
                                board,
                                self,
                                enemy,
                                publicBuildings,
                                config);
    }
    log.log(turnSystem.getTurnNumber(), self.id, "AI completed turn planning.");
}
