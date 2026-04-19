#include "Core/GameStateValidator.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace {
using CellPositionKey = std::pair<int, int>;

bool isValidKingdomId(KingdomId id) {
    return id == KingdomId::White || id == KingdomId::Black;
}

CellPositionKey makeCellPositionKey(const sf::Vector2i& position) {
    return {position.x, position.y};
}

std::string formatCellPosition(const sf::Vector2i& position) {
    return "(" + std::to_string(position.x) + "," + std::to_string(position.y) + ")";
}

std::string formatKingdomName(KingdomId kingdom) {
    return kingdom == KingdomId::White ? "White" : "Black";
}

std::string formatPieceLabel(KingdomId kingdom, int pieceId) {
    return formatKingdomName(kingdom) + " piece " + std::to_string(pieceId);
}

bool isSavePositionInBounds(const SaveData& data, const sf::Vector2i& position) {
    const int diameter = data.mapRadius * 2;
    return position.x >= 0 && position.x < diameter && position.y >= 0 && position.y < diameter;
}

bool validateAuthoritativeBuildingState(const Building& building,
                                        const std::string& scope,
                                        std::string* errorMessage) {
    if (!building.isUnderConstruction()) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = scope + " contains an under-construction building in authoritative state.";
    }
    return false;
}

bool isValidAutonomousUnitType(AutonomousUnitType type) {
    switch (type) {
        case AutonomousUnitType::InfernalPiece:
            return true;
    }

    return false;
}

bool isValidInfernalManifestedPieceType(PieceType type) {
    switch (type) {
        case PieceType::Pawn:
        case PieceType::Knight:
        case PieceType::Bishop:
        case PieceType::Rook:
        case PieceType::Queen:
            return true;

        case PieceType::King:
            return false;
    }

    return false;
}
}

void GameStateValidator::writeError(std::string* errorMessage, const std::string& message) {
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool GameStateValidator::validateKingdomParticipants(
    const std::array<KingdomParticipantConfig, kNumKingdoms>& participants,
    std::string* errorMessage) {
    std::set<int> seenKingdoms;

    for (const auto& participant : participants) {
        if (!isValidKingdomId(participant.kingdom)) {
            writeError(errorMessage, "Session contains an invalid kingdom identifier.");
            return false;
        }
        if (!isValidControllerType(participant.controller)) {
            writeError(errorMessage, "Session contains an invalid controller type.");
            return false;
        }
        if (participant.participantName.empty()) {
            writeError(errorMessage, "All kingdom participants must have a name.");
            return false;
        }

        seenKingdoms.insert(kingdomIndex(participant.kingdom));
    }

    if (seenKingdoms.size() != kNumKingdoms) {
        writeError(errorMessage, "Session must define exactly one participant per kingdom.");
        return false;
    }

    if (kingdomParticipantConfig(participants, KingdomId::White).kingdom != KingdomId::White
        || kingdomParticipantConfig(participants, KingdomId::Black).kingdom != KingdomId::Black) {
        writeError(errorMessage, "Session participant ordering must match White then Black kingdoms.");
        return false;
    }

    return true;
}

bool GameStateValidator::validateMultiplayerConfig(
    const std::array<KingdomParticipantConfig, kNumKingdoms>& participants,
    const MultiplayerConfig& multiplayer,
    std::string* errorMessage) {
    if (!multiplayer.enabled) {
        return true;
    }

    if (!supportsMultiplayerParticipants(participants)) {
        writeError(errorMessage, "Multiplayer sessions require two human-controlled kingdoms.");
        return false;
    }

    if (!isValidMultiplayerPort(multiplayer.port)) {
        writeError(errorMessage, "Multiplayer sessions require a valid LAN port between 1 and 65535.");
        return false;
    }

    if (multiplayer.passwordHash.empty()) {
        writeError(errorMessage, "Multiplayer sessions require a password hash.");
        return false;
    }

    if (multiplayer.passwordSalt.empty()) {
        writeError(errorMessage, "Multiplayer sessions require a password salt.");
        return false;
    }

    if (multiplayer.protocolVersion == 0) {
        writeError(errorMessage, "Multiplayer sessions require a valid protocol version.");
        return false;
    }

    return true;
}

bool GameStateValidator::validateSessionConfig(const GameSessionConfig& session, std::string* errorMessage) {
    if (session.saveName.empty()) {
        writeError(errorMessage, "Save name is required.");
        return false;
    }

    if (!validateKingdomParticipants(session.kingdoms, errorMessage)) {
        return false;
    }

    return validateMultiplayerConfig(session.kingdoms, session.multiplayer, errorMessage);
}

bool GameStateValidator::validateSaveData(const SaveData& data, std::string* errorMessage) {
    if (data.gameName.empty()) {
        writeError(errorMessage, "Save data is missing the game name.");
        return false;
    }
    if (data.mapRadius <= 0) {
        writeError(errorMessage, "Save data contains an invalid map radius.");
        return false;
    }
    if (!isValidKingdomId(data.activeKingdom)) {
        writeError(errorMessage, "Save data contains an invalid active kingdom.");
        return false;
    }
    if (data.turnNumber <= 0) {
        writeError(errorMessage, "Save data contains an invalid turn number.");
        return false;
    }
    if (!validateKingdomParticipants(data.sessionKingdoms, errorMessage)) {
        return false;
    }
    if (!validateMultiplayerConfig(data.sessionKingdoms, data.multiplayer, errorMessage)) {
        return false;
    }

    std::set<int> pieceIds;
    std::set<int> buildingIds;
    std::map<CellPositionKey, std::pair<KingdomId, int>> piecePositions;
    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        const auto expectedKingdom = static_cast<KingdomId>(kingdomSlot);
        const auto& kingdom = data.kingdoms[kingdomSlot];

        if (kingdom.id != expectedKingdom) {
            writeError(errorMessage, "Save data kingdom ordering is inconsistent.");
            return false;
        }
        if (kingdom.gold < 0) {
            writeError(errorMessage, "Save data contains negative gold for a kingdom.");
            return false;
        }
        if (kingdom.movementPointsMaxBonus < 0 || kingdom.buildPointsMaxBonus < 0) {
            writeError(errorMessage, "Save data contains negative permanent turn-point bonuses.");
            return false;
        }

        int kingCount = 0;
        for (const auto& piece : kingdom.pieces) {
            if (piece.kingdom != expectedKingdom) {
                writeError(errorMessage, "Save data contains a piece assigned to the wrong kingdom.");
                return false;
            }
            if (!pieceIds.insert(piece.id).second) {
                writeError(errorMessage, "Save data contains duplicate piece IDs.");
                return false;
            }
            if (!isSavePositionInBounds(data, piece.position)) {
                writeError(errorMessage,
                    "Save data contains a piece outside board bounds: "
                        + formatPieceLabel(piece.kingdom, piece.id) + " at "
                        + formatCellPosition(piece.position) + ".");
                return false;
            }
            const auto key = makeCellPositionKey(piece.position);
            const auto existingPiece = piecePositions.find(key);
            if (existingPiece != piecePositions.end()) {
                writeError(errorMessage,
                    "Save data contains overlapping pieces at " + formatCellPosition(piece.position)
                        + ": " + formatPieceLabel(existingPiece->second.first, existingPiece->second.second)
                        + " and " + formatPieceLabel(piece.kingdom, piece.id) + ".");
                return false;
            }
            piecePositions.emplace(key, std::make_pair(piece.kingdom, piece.id));
            if (piece.type == PieceType::King) {
                ++kingCount;
            }
        }

        if (kingCount != 1) {
            writeError(errorMessage, "Each kingdom must contain exactly one king in save data.");
            return false;
        }

        for (const auto& building : kingdom.buildings) {
            if (building.owner != expectedKingdom) {
                writeError(errorMessage, "Save data contains a building assigned to the wrong kingdom.");
                return false;
            }
            if (!validateAuthoritativeBuildingState(building, "Save data", errorMessage)) {
                return false;
            }
            if (!buildingIds.insert(building.id).second) {
                writeError(errorMessage, "Save data contains duplicate building IDs.");
                return false;
            }
            for (const auto& occupiedCell : building.getOccupiedCells()) {
                if (!isSavePositionInBounds(data, occupiedCell)) {
                    writeError(errorMessage,
                        "Save data contains a building outside board bounds: building "
                            + std::to_string(building.id) + " occupies "
                            + formatCellPosition(occupiedCell) + ".");
                    return false;
                }
            }
        }
    }

    for (const auto& building : data.publicBuildings) {
        if (!validateAuthoritativeBuildingState(building, "Save data", errorMessage)) {
            return false;
        }
        if (!buildingIds.insert(building.id).second) {
            writeError(errorMessage, "Save data contains duplicate building IDs.");
            return false;
        }
        for (const auto& occupiedCell : building.getOccupiedCells()) {
            if (!isSavePositionInBounds(data, occupiedCell)) {
                writeError(errorMessage,
                    "Save data contains a public building outside board bounds: building "
                        + std::to_string(building.id) + " occupies "
                        + formatCellPosition(occupiedCell) + ".");
                return false;
            }
        }
    }

    std::set<int> objectIds;
    std::map<CellPositionKey, int> mapObjectPositions;
    for (const auto& object : data.mapObjects) {
        if (!objectIds.insert(object.id).second) {
            writeError(errorMessage, "Save data contains duplicate map object IDs.");
            return false;
        }
        if (!isSavePositionInBounds(data, object.position)) {
            writeError(errorMessage,
                "Save data contains a map object outside board bounds: object "
                    + std::to_string(object.id) + " at " + formatCellPosition(object.position) + ".");
            return false;
        }
        const auto key = makeCellPositionKey(object.position);
        const auto existingObject = mapObjectPositions.find(key);
        if (existingObject != mapObjectPositions.end()) {
            writeError(errorMessage,
                "Save data contains overlapping map objects at " + formatCellPosition(object.position)
                    + ": object " + std::to_string(existingObject->second)
                    + " and object " + std::to_string(object.id) + ".");
            return false;
        }
        mapObjectPositions.emplace(key, object.id);
    }

    std::set<int> autonomousUnitIds;
    std::map<CellPositionKey, int> autonomousUnitPositions;
    for (const auto& autonomousUnit : data.autonomousUnits) {
        if (!isValidAutonomousUnitType(autonomousUnit.type)) {
            writeError(errorMessage, "Save data contains an invalid autonomous unit type.");
            return false;
        }
        if (!autonomousUnitIds.insert(autonomousUnit.id).second) {
            writeError(errorMessage, "Save data contains duplicate autonomous unit IDs.");
            return false;
        }
        if (!isSavePositionInBounds(data, autonomousUnit.position)) {
            writeError(errorMessage,
                "Save data contains an autonomous unit outside board bounds: unit "
                    + std::to_string(autonomousUnit.id) + " at "
                    + formatCellPosition(autonomousUnit.position) + ".");
            return false;
        }
        const auto key = makeCellPositionKey(autonomousUnit.position);
        const auto existingUnit = autonomousUnitPositions.find(key);
        if (existingUnit != autonomousUnitPositions.end()) {
            writeError(errorMessage,
                "Save data contains overlapping autonomous units at "
                    + formatCellPosition(autonomousUnit.position) + ": unit "
                    + std::to_string(existingUnit->second) + " and unit "
                    + std::to_string(autonomousUnit.id) + ".");
            return false;
        }
        autonomousUnitPositions.emplace(key, autonomousUnit.id);
        if (!isValidKingdomId(autonomousUnit.infernal.targetKingdom)) {
            writeError(errorMessage, "Save data contains an autonomous unit with an invalid target kingdom.");
            return false;
        }
        if (!isValidInfernalManifestedPieceType(autonomousUnit.infernal.manifestedPieceType)) {
            writeError(errorMessage, "Save data contains an infernal autonomous unit with an invalid manifested piece type.");
            return false;
        }
    }

    if (data.infernalSystemState.activeInfernalUnitId >= 0
        && autonomousUnitIds.count(data.infernalSystemState.activeInfernalUnitId) == 0) {
        writeError(errorMessage, "Save data infernal state references a missing active autonomous unit.");
        return false;
    }
    if (data.infernalSystemState.whiteBloodDebt < 0 || data.infernalSystemState.blackBloodDebt < 0) {
        writeError(errorMessage, "Save data contains negative infernal blood debt.");
        return false;
    }

    return true;
}

bool GameStateValidator::validateRuntimeState(const Board& board,
                                             const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                             const std::vector<Building>& publicBuildings,
                                             const std::vector<MapObject>& mapObjects,
                                             const std::vector<AutonomousUnit>& autonomousUnits,
                                             const TurnSystem& turnSystem,
                                             const GameSessionConfig& session,
                                             const XPSystemState& xpSystemState,
                                             const InfernalSystemState& infernalSystemState,
                                             std::string* errorMessage) {
    if (!validateSessionConfig(session, errorMessage)) {
        return false;
    }

    if (!isValidKingdomId(turnSystem.getActiveKingdom())) {
        writeError(errorMessage, "Runtime state contains an invalid active kingdom.");
        return false;
    }
    if (turnSystem.getTurnNumber() <= 0) {
        writeError(errorMessage, "Runtime state contains an invalid turn number.");
        return false;
    }

    (void)xpSystemState;

    std::set<int> pieceIds;
    std::set<int> buildingIds;
    std::map<CellPositionKey, std::pair<KingdomId, int>> piecePositions;
    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        const auto expectedKingdom = static_cast<KingdomId>(kingdomSlot);
        const auto& kingdom = kingdoms[kingdomSlot];

        if (kingdom.id != expectedKingdom) {
            writeError(errorMessage, "Runtime kingdom ordering is inconsistent.");
            return false;
        }
        if (kingdom.gold < 0) {
            writeError(errorMessage, "Runtime state contains negative gold for a kingdom.");
            return false;
        }
        if (kingdom.movementPointsMaxBonus < 0 || kingdom.buildPointsMaxBonus < 0) {
            writeError(errorMessage, "Runtime state contains negative permanent turn-point bonuses.");
            return false;
        }

        int kingCount = 0;
        for (const auto& piece : kingdom.pieces) {
            if (piece.kingdom != expectedKingdom) {
                writeError(errorMessage, "Runtime state contains a piece assigned to the wrong kingdom.");
                return false;
            }
            if (!board.isInBounds(piece.position.x, piece.position.y)) {
                writeError(errorMessage, "Runtime state contains a piece outside board bounds.");
                return false;
            }
            if (!pieceIds.insert(piece.id).second) {
                writeError(errorMessage, "Runtime state contains duplicate piece IDs.");
                return false;
            }
            const auto key = makeCellPositionKey(piece.position);
            const auto existingPiece = piecePositions.find(key);
            if (existingPiece != piecePositions.end()) {
                writeError(errorMessage,
                    "Runtime state contains overlapping pieces at " + formatCellPosition(piece.position)
                        + ": " + formatPieceLabel(existingPiece->second.first, existingPiece->second.second)
                        + " and " + formatPieceLabel(piece.kingdom, piece.id) + ".");
                return false;
            }
            piecePositions.emplace(key, std::make_pair(piece.kingdom, piece.id));
            const Cell& cell = board.getCell(piece.position.x, piece.position.y);
            if (cell.piece == nullptr) {
                writeError(errorMessage,
                    "Runtime board cell pointer is missing for "
                        + formatPieceLabel(piece.kingdom, piece.id) + " at "
                        + formatCellPosition(piece.position) + ".");
                return false;
            }
            if (cell.piece->id != piece.id) {
                writeError(errorMessage,
                    "Runtime board cell pointer mismatch at " + formatCellPosition(piece.position)
                        + ": expected " + formatPieceLabel(piece.kingdom, piece.id)
                        + ", found " + formatPieceLabel(cell.piece->kingdom, cell.piece->id) + ".");
                return false;
            }
            if (piece.type == PieceType::King) {
                ++kingCount;
            }
        }

        if (kingCount != 1) {
            writeError(errorMessage, "Runtime state must contain exactly one king per kingdom.");
            return false;
        }

        for (const auto& building : kingdom.buildings) {
            if (building.owner != expectedKingdom) {
                writeError(errorMessage, "Runtime state contains an owned building assigned to the wrong kingdom.");
                return false;
            }
            if (!validateAuthoritativeBuildingState(building, "Runtime state", errorMessage)) {
                return false;
            }
            if (!buildingIds.insert(building.id).second) {
                writeError(errorMessage, "Runtime state contains duplicate building IDs.");
                return false;
            }
            for (const auto& occupiedCell : building.getOccupiedCells()) {
                if (!board.isInBounds(occupiedCell.x, occupiedCell.y)) {
                    writeError(errorMessage, "Runtime state contains a building outside board bounds.");
                    return false;
                }
            }
        }
    }

    for (const auto& building : publicBuildings) {
        if (!validateAuthoritativeBuildingState(building, "Runtime state", errorMessage)) {
            return false;
        }
        if (!buildingIds.insert(building.id).second) {
            writeError(errorMessage, "Runtime state contains duplicate building IDs.");
            return false;
        }
        for (const auto& occupiedCell : building.getOccupiedCells()) {
            if (!board.isInBounds(occupiedCell.x, occupiedCell.y)) {
                writeError(errorMessage, "Runtime state contains a public building outside board bounds.");
                return false;
            }
        }
    }

    std::set<int> objectIds;
    std::map<CellPositionKey, int> mapObjectPositions;
    for (const auto& object : mapObjects) {
        if (!board.isInBounds(object.position.x, object.position.y)) {
            writeError(errorMessage, "Runtime state contains a map object outside board bounds.");
            return false;
        }
        if (!objectIds.insert(object.id).second) {
            writeError(errorMessage, "Runtime state contains duplicate map object IDs.");
            return false;
        }
        const auto key = makeCellPositionKey(object.position);
        const auto existingObject = mapObjectPositions.find(key);
        if (existingObject != mapObjectPositions.end()) {
            writeError(errorMessage,
                "Runtime state contains overlapping map objects at " + formatCellPosition(object.position)
                    + ": object " + std::to_string(existingObject->second)
                    + " and object " + std::to_string(object.id) + ".");
            return false;
        }
        mapObjectPositions.emplace(key, object.id);
        const Cell& cell = board.getCell(object.position.x, object.position.y);
        if (cell.mapObject == nullptr) {
            writeError(errorMessage,
                "Runtime board cell pointer is missing for map object "
                    + std::to_string(object.id) + " at " + formatCellPosition(object.position) + ".");
            return false;
        }
        if (cell.mapObject->id != object.id) {
            writeError(errorMessage,
                "Runtime board cell pointer mismatch at " + formatCellPosition(object.position)
                    + ": expected map object " + std::to_string(object.id)
                    + ", found map object " + std::to_string(cell.mapObject->id) + ".");
            return false;
        }
    }

    std::set<int> autonomousUnitIds;
    std::map<CellPositionKey, int> autonomousUnitPositions;
    for (const auto& autonomousUnit : autonomousUnits) {
        if (!isValidAutonomousUnitType(autonomousUnit.type)) {
            writeError(errorMessage, "Runtime state contains an invalid autonomous unit type.");
            return false;
        }
        if (!isValidKingdomId(autonomousUnit.infernal.targetKingdom)) {
            writeError(errorMessage, "Runtime state contains an autonomous unit with an invalid target kingdom.");
            return false;
        }
        if (!isValidInfernalManifestedPieceType(autonomousUnit.infernal.manifestedPieceType)) {
            writeError(errorMessage, "Runtime state contains an infernal autonomous unit with an invalid manifested piece type.");
            return false;
        }
        if (!board.isInBounds(autonomousUnit.position.x, autonomousUnit.position.y)) {
            writeError(errorMessage, "Runtime state contains an autonomous unit outside board bounds.");
            return false;
        }
        if (!autonomousUnitIds.insert(autonomousUnit.id).second) {
            writeError(errorMessage, "Runtime state contains duplicate autonomous unit IDs.");
            return false;
        }
        const auto key = makeCellPositionKey(autonomousUnit.position);
        const auto existingUnit = autonomousUnitPositions.find(key);
        if (existingUnit != autonomousUnitPositions.end()) {
            writeError(errorMessage,
                "Runtime state contains overlapping autonomous units at "
                    + formatCellPosition(autonomousUnit.position) + ": unit "
                    + std::to_string(existingUnit->second) + " and unit "
                    + std::to_string(autonomousUnit.id) + ".");
            return false;
        }
        autonomousUnitPositions.emplace(key, autonomousUnit.id);

        const Cell& cell = board.getCell(autonomousUnit.position.x, autonomousUnit.position.y);
        if (cell.autonomousUnit == nullptr) {
            writeError(errorMessage,
                "Runtime board cell pointer is missing for autonomous unit "
                    + std::to_string(autonomousUnit.id) + " at "
                    + formatCellPosition(autonomousUnit.position) + ".");
            return false;
        }
        if (cell.autonomousUnit->id != autonomousUnit.id) {
            writeError(errorMessage,
                "Runtime board cell pointer mismatch at " + formatCellPosition(autonomousUnit.position)
                    + ": expected autonomous unit " + std::to_string(autonomousUnit.id)
                    + ", found autonomous unit " + std::to_string(cell.autonomousUnit->id) + ".");
            return false;
        }
    }

    if (infernalSystemState.activeInfernalUnitId >= 0
        && autonomousUnitIds.count(infernalSystemState.activeInfernalUnitId) == 0) {
        writeError(errorMessage, "Runtime infernal state references a missing active autonomous unit.");
        return false;
    }
    if (infernalSystemState.whiteBloodDebt < 0 || infernalSystemState.blackBloodDebt < 0) {
        writeError(errorMessage, "Runtime infernal state contains negative blood debt.");
        return false;
    }

    return true;
}