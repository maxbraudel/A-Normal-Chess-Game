#pragma once
#include <vector>
#include <string>
#include "Kingdom/KingdomId.hpp"
#include "Board/Cell.hpp"
#include "Units/Piece.hpp"
#include "Buildings/Building.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/TurnCommand.hpp"

struct SaveData {
    std::string gameName;
    int turnNumber = 1;
    KingdomId activeKingdom = KingdomId::White;
    int mapRadius = 50;

    // Grid state
    struct CellData {
        CellType type = CellType::Grass;
        bool isInCircle = false;
    };
    std::vector<std::vector<CellData>> grid;

    // Kingdom data
    struct KingdomData {
        KingdomId id = KingdomId::White;
        int gold = 0;
        std::vector<Piece> pieces;
        std::vector<Building> buildings;
    };
    KingdomData whiteKingdom;
    KingdomData blackKingdom;

    // Public buildings
    std::vector<Building> publicBuildings;

    // Events
    std::vector<EventLog::Event> events;

    // Command history (for future replay)
    std::vector<TurnCommand> commandHistory;
};
