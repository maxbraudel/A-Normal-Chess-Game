#pragma once

#include <array>
#include <ctime>
#include <string>
#include <vector>
#include "Kingdom/Kingdom.hpp"

class GameStateDebugRecorder {
public:
    void reset();
    void logTurnState(int turnNumber,
                      const std::array<Kingdom, kNumKingdoms>& kingdoms,
                      const std::string& reason) const;
    void recordSnapshot(int turnNumber,
                        KingdomId activeKingdom,
                        const std::array<Kingdom, kNumKingdoms>& kingdoms,
                        const std::string& reason);
    void exportHistory(const std::string& gameName,
                       int currentTurn,
                       KingdomId activeKingdom) const;

private:
    struct DebugPieceState {
        int id = -1;
        PieceType type = PieceType::Pawn;
        sf::Vector2i position{0, 0};
        int xp = 0;
    };

    struct DebugBuildingState {
        int id = -1;
        BuildingType type = BuildingType::Barracks;
        sf::Vector2i origin{0, 0};
        int width = 1;
        int height = 1;
        bool isProducing = false;
        int turnsRemaining = 0;
    };

    struct DebugKingdomState {
        KingdomId id = KingdomId::White;
        int gold = 0;
        std::vector<DebugPieceState> pieces;
        std::vector<DebugBuildingState> buildings;
    };

    struct DebugTurnSnapshot {
        int turnNumber = 0;
        KingdomId activeKingdom = KingdomId::White;
        std::string reason;
        std::time_t capturedAt = 0;
        std::array<DebugKingdomState, kNumKingdoms> kingdoms;
    };

    std::vector<DebugTurnSnapshot> m_history;
};