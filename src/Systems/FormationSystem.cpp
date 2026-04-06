#include "Systems/FormationSystem.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Units/Piece.hpp"
#include <algorithm>
#include <cmath>

int FormationSystem::s_nextFormationId = 1;

bool FormationSystem::canFormGroup(const Kingdom& kingdom, const std::vector<int>& pieceIds, const Board& board) {
    if (pieceIds.size() < 2) return false;
    return areAdjacent(board, pieceIds, kingdom);
}

int FormationSystem::formGroup(Kingdom& kingdom, const std::vector<int>& pieceIds) {
    int fid = s_nextFormationId++;
    for (int pid : pieceIds) {
        Piece* p = kingdom.getPieceById(pid);
        if (p) p->formationId = fid;
    }
    return fid;
}

void FormationSystem::breakGroup(Kingdom& kingdom, int formationId) {
    for (auto& p : kingdom.pieces) {
        if (p.formationId == formationId) {
            p.formationId = -1;
        }
    }
}

std::vector<int> FormationSystem::getFormationPieceIds(const Kingdom& kingdom, int formationId) {
    std::vector<int> ids;
    for (const auto& p : kingdom.pieces) {
        if (p.formationId == formationId) ids.push_back(p.id);
    }
    return ids;
}

bool FormationSystem::areAdjacent(const Board& board, const std::vector<int>& pieceIds, const Kingdom& kingdom) {
    // Check that all pieces form a connected group via 4-directional adjacency
    if (pieceIds.empty()) return false;

    std::vector<sf::Vector2i> positions;
    for (int pid : pieceIds) {
        const Piece* p = nullptr;
        for (const auto& piece : kingdom.pieces) {
            if (piece.id == pid) { p = &piece; break; }
        }
        if (!p) return false;
        positions.push_back(p->position);
    }

    // BFS connectivity check
    std::vector<bool> visited(positions.size(), false);
    std::vector<int> stack;
    stack.push_back(0);
    visited[0] = true;
    int count = 1;

    while (!stack.empty()) {
        int idx = stack.back();
        stack.pop_back();
        for (int i = 0; i < static_cast<int>(positions.size()); ++i) {
            if (visited[i]) continue;
            int dx = std::abs(positions[idx].x - positions[i].x);
            int dy = std::abs(positions[idx].y - positions[i].y);
            if ((dx == 1 && dy == 0) || (dx == 0 && dy == 1)) {
                visited[i] = true;
                ++count;
                stack.push_back(i);
            }
        }
    }

    return count == static_cast<int>(positions.size());
}
