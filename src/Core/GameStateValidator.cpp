#include "Core/GameStateValidator.hpp"

#include <set>

namespace {
bool isValidKingdomId(KingdomId id) {
    return id == KingdomId::White || id == KingdomId::Black;
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
    }

    return true;
}

bool GameStateValidator::validateRuntimeState(const Board& board,
                                             const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                             const std::vector<Building>& publicBuildings,
                                             const TurnSystem& turnSystem,
                                             const GameSessionConfig& session,
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

    std::set<int> pieceIds;
    std::set<int> buildingIds;
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
            const Cell& cell = board.getCell(piece.position.x, piece.position.y);
            if (cell.piece == nullptr || cell.piece->id != piece.id) {
                writeError(errorMessage, "Runtime board cell pointers are out of sync for a piece.");
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

    return true;
}