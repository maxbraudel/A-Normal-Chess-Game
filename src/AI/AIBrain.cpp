#include "AI/AIBrain.hpp"
#include "AI/AITurnContext.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include <cmath>

AIBrain::AIBrain()
    : m_phase(AIPhase::EARLY_GAME), m_turnNumber(0) {}

void AIBrain::update(const Board& board, const Kingdom& self, const Kingdom& enemy,
                     const GameConfig& config, const AITurnContext& ctx, int turnNumber) {
    m_turnNumber = turnNumber;
    determinePhase(board, self, enemy, config, ctx);
    setPrioritiesForPhase();
}

AIPhase AIBrain::getPhase() const { return m_phase; }
const PhasePriorities& AIBrain::getPriorities() const { return m_priorities; }

float AIBrain::getMaterialScore(const Kingdom& kingdom) const {
    float score = 0.0f;
    for (const auto& p : kingdom.pieces) {
        switch (p.type) {
            case PieceType::Pawn:   score += 100.0f; break;
            case PieceType::Knight: score += 320.0f; break;
            case PieceType::Bishop: score += 330.0f; break;
            case PieceType::Rook:   score += 500.0f; break;
            case PieceType::Queen:  score += 900.0f; break;
            case PieceType::King:   break; // King is always present
            default: break;
        }
    }
    return score;
}

int AIBrain::countCombatPieces(const Kingdom& kingdom) const {
    int count = 0;
    for (const auto& p : kingdom.pieces) {
        if (p.type != PieceType::King) ++count;
    }
    return count;
}

bool AIBrain::hasIncome(const Kingdom& kingdom, const Board& board) const {
    for (const auto& p : kingdom.pieces) {
        const Cell& cell = board.getCell(p.position.x, p.position.y);
        if (cell.building &&
            (cell.building->type == BuildingType::Mine || cell.building->type == BuildingType::Farm)) {
            return true;
        }
    }
    return false;
}

bool AIBrain::hasSufficientMatingMaterial(const Kingdom& kingdom) const {
    // Check if we have enough material to deliver checkmate
    int knights = 0, bishops = 0, rooks = 0, queens = 0, pawns = 0;
    for (const auto& p : kingdom.pieces) {
        switch (p.type) {
            case PieceType::Knight: ++knights; break;
            case PieceType::Bishop: ++bishops; break;
            case PieceType::Rook:   ++rooks;   break;
            case PieceType::Queen:  ++queens;   break;
            case PieceType::Pawn:   ++pawns;    break;
            default: break;
        }
    }
    // Queen or Rook alone can mate
    if (queens > 0 || rooks > 0) return true;
    // Two bishops can mate
    if (bishops >= 2) return true;
    // Bishop + knight can mate
    if (bishops >= 1 && knights >= 1) return true;
    // Pawns can promote eventually
    if (pawns > 0) return true;
    // Single knight or single bishop cannot mate
    return false;
}

void AIBrain::determinePhase(const Board& board, const Kingdom& self, const Kingdom& enemy,
                             const GameConfig& config, const AITurnContext& ctx) {
    // CRISIS: only when king is actually in check (standing on a threatened square)
    const Piece* king = self.getKing();
    if (king && ctx.enemyThreats.isSet(king->position)) {
        m_phase = AIPhase::CRISIS;
        return;
    }

    int myCombat = countCombatPieces(self);
    int enemyCombat = countCombatPieces(enemy);
    float myMaterial = getMaterialScore(self);
    float enemyMaterial = getMaterialScore(enemy);

    // ENDGAME: few pieces on both sides — but only if we actually built up first
    if (myCombat <= 3 && enemyCombat <= 3 && m_turnNumber > 15) {
        bool hasBarracks = false;
        for (const auto& b : self.buildings) {
            if (b.type == BuildingType::Barracks && !b.isDestroyed()) { hasBarracks = true; break; }
        }
        if (myCombat > 0 || enemyCombat > 0 || hasBarracks) {
            m_phase = AIPhase::ENDGAME;
            return;
        }
    }

    // AGGRESSION: material advantage OR large army — but must have mating material
    if (hasSufficientMatingMaterial(self) &&
        ((myMaterial > enemyMaterial * 1.3f && myCombat >= 3)
        || (myCombat >= 5)
        || (myCombat >= 3 && m_turnNumber > 25))) {
        m_phase = AIPhase::AGGRESSION;
        return;
    }

    // EARLY_GAME: few pieces, early turns
    if (m_turnNumber <= 10 && myCombat <= 2) {
        m_phase = AIPhase::EARLY_GAME;
        return;
    }

    // MID_GAME: have reasonable army AND mating material
    if (hasSufficientMatingMaterial(self) && (myCombat >= 3 || (myCombat >= 2 && m_turnNumber > 15))) {
        m_phase = AIPhase::MID_GAME;
        return;
    }

    // BUILD_UP: default when not in other phases
    m_phase = AIPhase::BUILD_UP;
}

void AIBrain::setPrioritiesForPhase() {
    switch (m_phase) {
        case AIPhase::EARLY_GAME:
            m_priorities = {0.9f, 0.8f, 0.7f, 0.0f, 0.3f};
            break;
        case AIPhase::BUILD_UP:
            m_priorities = {0.7f, 0.9f, 0.6f, 0.2f, 0.5f};
            break;
        case AIPhase::MID_GAME:
            m_priorities = {0.4f, 0.7f, 0.4f, 0.7f, 0.6f};
            break;
        case AIPhase::AGGRESSION:
            m_priorities = {0.2f, 0.5f, 0.2f, 1.0f, 0.4f};
            break;
        case AIPhase::ENDGAME:
            m_priorities = {0.3f, 0.3f, 0.3f, 1.0f, 0.8f};
            break;
        case AIPhase::CRISIS:
            m_priorities = {0.0f, 0.0f, 0.0f, 0.3f, 1.0f};
            break;
    }
}

std::string AIBrain::getPhaseName() const {
    switch (m_phase) {
        case AIPhase::EARLY_GAME: return "EARLY_GAME";
        case AIPhase::BUILD_UP:   return "BUILD_UP";
        case AIPhase::MID_GAME:   return "MID_GAME";
        case AIPhase::AGGRESSION: return "AGGRESSION";
        case AIPhase::ENDGAME:    return "ENDGAME";
        case AIPhase::CRISIS:     return "CRISIS";
    }
    return "UNKNOWN";
}
