#include "Save/SaveManager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

void skipWhitespace(const std::string& text, std::size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
}

std::size_t findValueStart(const std::string& json, const std::string& key) {
    const std::string search = "\"" + key + "\"";
    std::size_t pos = json.find(search);
    if (pos == std::string::npos) {
        return std::string::npos;
    }

    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) {
        return std::string::npos;
    }

    ++pos;
    skipWhitespace(json, pos);
    return pos;
}

std::size_t findMatchingDelimiter(const std::string& text,
                                  std::size_t start,
                                  char open,
                                  char close) {
    if (start >= text.size() || text[start] != open) {
        return std::string::npos;
    }

    bool inString = false;
    int depth = 0;
    for (std::size_t pos = start; pos < text.size(); ++pos) {
        const char current = text[pos];
        if (current == '"' && (pos == 0 || text[pos - 1] != '\\')) {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (current == open) {
            ++depth;
        } else if (current == close) {
            --depth;
            if (depth == 0) {
                return pos;
            }
        }
    }

    return std::string::npos;
}

std::string trim(const std::string& value) {
    std::size_t start = 0;
    std::size_t end = value.size();
    while (start < end && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string unescapeJsonString(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char current = value[i];
        if (current != '\\' || i + 1 >= value.size()) {
            result.push_back(current);
            continue;
        }

        const char escaped = value[++i];
        switch (escaped) {
            case '\\': result.push_back('\\'); break;
            case '"': result.push_back('"'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            default: result.push_back(escaped); break;
        }
    }
    return result;
}

void writeUInt8Array(std::ostream& output, const std::vector<std::uint8_t>& values) {
    output << "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            output << ",";
        }
        output << static_cast<int>(values[i]);
    }
    output << "]";
}

std::vector<std::uint8_t> parseUInt8Array(const std::string& arrayText) {
    std::vector<std::uint8_t> values;
    if (arrayText.size() < 2 || arrayText.front() != '[' || arrayText.back() != ']') {
        return values;
    }

    std::size_t pos = 1;
    while (pos < arrayText.size()) {
        while (pos < arrayText.size()
               && (std::isspace(static_cast<unsigned char>(arrayText[pos])) != 0
                   || arrayText[pos] == ',')) {
            ++pos;
        }

        if (pos >= arrayText.size() || arrayText[pos] == ']') {
            break;
        }

        std::size_t end = pos;
        if (arrayText[end] == '-' || arrayText[end] == '+') {
            ++end;
        }
        while (end < arrayText.size()
               && std::isdigit(static_cast<unsigned char>(arrayText[end])) != 0) {
            ++end;
        }

        try {
            values.push_back(static_cast<std::uint8_t>(std::clamp(
                std::stoi(arrayText.substr(pos, end - pos)),
                0,
                255)));
        } catch (...) {
            break;
        }
        pos = end;
    }

    return values;
}

}

void SaveManager::writeJson(std::ostream& output, const SaveData& data) {
    output << "{\n";
    output << "  \"gameName\": \"" << SaveManager::escapeJsonString(data.gameName) << "\",\n";
    output << "  \"turnNumber\": " << data.turnNumber << ",\n";
    output << "  \"activeKingdom\": " << static_cast<int>(data.activeKingdom) << ",\n";
    output << "  \"mapRadius\": " << data.mapRadius << ",\n";
    output << "  \"worldSeed\": " << data.worldSeed << ",\n";
    output << "  \"sessionKingdoms\": [";
    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        if (kingdomSlot > 0) output << ", ";
        output << SaveManager::serializeParticipant(data.sessionKingdoms[kingdomSlot]);
    }
    output << "],\n";
    output << "  \"multiplayer\": " << SaveManager::serializeMultiplayerConfig(data.multiplayer) << ",\n";

    output << "  \"grid\": [\n";
    for (std::size_t y = 0; y < data.grid.size(); ++y) {
        output << "    [";
        for (std::size_t x = 0; x < data.grid[y].size(); ++x) {
            output << "{\"t\":" << static_cast<int>(data.grid[y][x].type)
                   << ",\"c\":" << (data.grid[y][x].isInCircle ? 1 : 0)
                   << ",\"f\":" << data.grid[y][x].terrainFlipMask
                   << ",\"b\":" << static_cast<int>(data.grid[y][x].terrainBrightness)
                   << "}";
            if (x + 1 < data.grid[y].size()) output << ",";
        }
        output << "]";
        if (y + 1 < data.grid.size()) output << ",";
        output << "\n";
    }
    output << "  ],\n";

    static const char* kingdomKeys[] = {"whiteKingdom", "blackKingdom"};
    for (int k = 0; k < kNumKingdoms; ++k) {
        const auto& kd = data.kingdoms[k];
        output << "  \"" << kingdomKeys[k] << "\": {\n";
        output << "    \"gold\": " << kd.gold << ",\n";
        output << "    \"movementPointsMaxBonus\": " << kd.movementPointsMaxBonus << ",\n";
        output << "    \"buildPointsMaxBonus\": " << kd.buildPointsMaxBonus << ",\n";
        output << "    \"hasSpawnedBishop\": " << (kd.hasSpawnedBishop ? 1 : 0) << ",\n";
        output << "    \"lastBishopSpawnParity\": " << kd.lastBishopSpawnParity << ",\n";
        output << "    \"pieces\": [";
        for (std::size_t i = 0; i < kd.pieces.size(); ++i) {
            if (i > 0) output << ", ";
            output << SaveManager::serializePiece(kd.pieces[i]);
        }
        output << "],\n";
        output << "    \"buildings\": [";
        for (std::size_t i = 0; i < kd.buildings.size(); ++i) {
            if (i > 0) output << ", ";
            output << SaveManager::serializeBuilding(kd.buildings[i]);
        }
        output << "]\n  }";
        output << ",\n";
    }

    output << "  \"publicBuildings\": [";
    for (std::size_t i = 0; i < data.publicBuildings.size(); ++i) {
        if (i > 0) output << ", ";
        output << SaveManager::serializeBuilding(data.publicBuildings[i]);
    }
    output << "],\n";

    output << "  \"mapObjects\": [";
    for (std::size_t i = 0; i < data.mapObjects.size(); ++i) {
        if (i > 0) output << ", ";
        output << SaveManager::serializeMapObject(data.mapObjects[i]);
    }
    output << "],\n";

    output << "  \"autonomousUnits\": [";
    for (std::size_t i = 0; i < data.autonomousUnits.size(); ++i) {
        if (i > 0) output << ", ";
        output << SaveManager::serializeAutonomousUnit(data.autonomousUnits[i]);
    }
    output << "],\n";

    output << "  \"chestState\": {"
           << "\"activeChestObjectId\":" << data.chestSystemState.activeChestObjectId << ","
           << "\"nextSpawnTurn\":" << data.chestSystemState.nextSpawnTurn << ","
           << "\"rngCounter\":" << data.chestSystemState.rngCounter
           << "},\n";

        output << "  \"weatherState\": {"
            << "\"nextSpawnTurnStep\":" << data.weatherSystemState.nextSpawnTurnStep << ","
            << "\"hasActiveFront\":" << (data.weatherSystemState.hasActiveFront ? 1 : 0) << ","
            << "\"rngCounter\":" << data.weatherSystemState.rngCounter << ","
            << "\"revision\":" << data.weatherSystemState.revision << ","
            << "\"activeFront\": {"
            << "\"direction\":" << static_cast<int>(data.weatherSystemState.activeFront.direction) << ","
            << "\"currentTurnStep\":" << data.weatherSystemState.activeFront.currentTurnStep << ","
            << "\"totalTurnSteps\":" << data.weatherSystemState.activeFront.totalTurnSteps << ","
            << "\"centerStartXTimes1000\":" << data.weatherSystemState.activeFront.centerStartXTimes1000 << ","
            << "\"centerStartYTimes1000\":" << data.weatherSystemState.activeFront.centerStartYTimes1000 << ","
            << "\"stepXTimes1000\":" << data.weatherSystemState.activeFront.stepXTimes1000 << ","
            << "\"stepYTimes1000\":" << data.weatherSystemState.activeFront.stepYTimes1000 << ","
            << "\"radiusAlongTimes1000\":" << data.weatherSystemState.activeFront.radiusAlongTimes1000 << ","
            << "\"radiusAcrossTimes1000\":" << data.weatherSystemState.activeFront.radiusAcrossTimes1000 << ","
            << "\"shapeSeed\":" << data.weatherSystemState.activeFront.shapeSeed << ","
            << "\"densitySeed\":" << data.weatherSystemState.activeFront.densitySeed
            << "},"
            << "\"mask\": {"
            << "\"revision\":" << data.weatherMaskCache.revision << ","
            << "\"diameter\":" << data.weatherMaskCache.diameter << ","
            << "\"hasActiveFront\":" << (data.weatherMaskCache.hasActiveFront ? 1 : 0) << ","
            << "\"alphaByCell\":";
        writeUInt8Array(output, data.weatherMaskCache.alphaByCell);
        output << ",\"shadeByCell\":";
        writeUInt8Array(output, data.weatherMaskCache.shadeByCell);
        output << "}"
            << "},\n";

        output << "  \"xpState\": {"
            << "\"rngCounter\":" << data.xpSystemState.rngCounter
            << "},\n";

    output << "  \"infernalState\": {"
           << "\"activeInfernalUnitId\":" << data.infernalSystemState.activeInfernalUnitId << ","
           << "\"nextSpawnTurn\":" << data.infernalSystemState.nextSpawnTurn << ","
           << "\"whiteBloodDebt\":" << data.infernalSystemState.whiteBloodDebt << ","
           << "\"blackBloodDebt\":" << data.infernalSystemState.blackBloodDebt << ","
           << "\"rngCounter\":" << data.infernalSystemState.rngCounter
           << "},\n";

    output << "  \"events\": [";
    for (std::size_t i = 0; i < data.events.size(); ++i) {
        if (i > 0) output << ", ";
        output << SaveManager::serializeEvent(data.events[i]);
    }
    output << "]\n";

    output << "}\n";
}

std::string SaveManager::escapeJsonString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char current : value) {
        switch (current) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(current); break;
        }
    }
    return escaped;
}

std::string SaveManager::extractString(const std::string& json, const std::string& key) {
    std::size_t pos = findValueStart(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"') {
        return "";
    }

    ++pos;
    const std::size_t start = pos;
    while (pos < json.size()) {
        if (json[pos] == '"' && json[pos - 1] != '\\') {
            return unescapeJsonString(json.substr(start, pos - start));
        }
        ++pos;
    }

    return "";
}

int SaveManager::extractInt(const std::string& json, const std::string& key, int defaultVal) {
    std::size_t pos = findValueStart(json, key);
    if (pos == std::string::npos) {
        return defaultVal;
    }

    std::size_t end = pos;
    if (end < json.size() && (json[end] == '-' || json[end] == '+')) {
        ++end;
    }
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end])) != 0) {
        ++end;
    }
    if (end == pos) {
        return defaultVal;
    }

    try {
        return std::stoi(json.substr(pos, end - pos));
    } catch (...) {
        return defaultVal;
    }
}

bool SaveManager::extractBool(const std::string& json, const std::string& key, bool defaultVal) {
    std::size_t pos = findValueStart(json, key);
    if (pos == std::string::npos) {
        return defaultVal;
    }
    if (json.compare(pos, 4, "true") == 0) {
        return true;
    }
    if (json.compare(pos, 5, "false") == 0) {
        return false;
    }
    return defaultVal;
}

std::string SaveManager::extractSection(const std::string& json, const std::string& key) {
    std::size_t pos = findValueStart(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '{') {
        return "{}";
    }
    const std::size_t end = findMatchingDelimiter(json, pos, '{', '}');
    if (end == std::string::npos) {
        return "{}";
    }
    return json.substr(pos, end - pos + 1);
}

std::string SaveManager::extractArray(const std::string& json, const std::string& key) {
    std::size_t pos = findValueStart(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '[') {
        return "[]";
    }
    const std::size_t end = findMatchingDelimiter(json, pos, '[', ']');
    if (end == std::string::npos) {
        return "[]";
    }
    return json.substr(pos, end - pos + 1);
}

std::vector<std::string> SaveManager::splitArrayElements(const std::string& arrayContent) {
    std::vector<std::string> result;
    if (arrayContent.size() < 2 || arrayContent.front() != '[' || arrayContent.back() != ']') {
        return result;
    }

    bool inString = false;
    int braceDepth = 0;
    int bracketDepth = 0;
    std::size_t elementStart = 1;

    for (std::size_t pos = 1; pos + 1 < arrayContent.size(); ++pos) {
        const char current = arrayContent[pos];
        if (current == '"' && arrayContent[pos - 1] != '\\') {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (current == '{') {
            ++braceDepth;
        } else if (current == '}') {
            --braceDepth;
        } else if (current == '[') {
            ++bracketDepth;
        } else if (current == ']') {
            --bracketDepth;
        } else if (current == ',' && braceDepth == 0 && bracketDepth == 0) {
            const std::string element = trim(arrayContent.substr(elementStart, pos - elementStart));
            if (!element.empty()) {
                result.push_back(element);
            }
            elementStart = pos + 1;
        }
    }

    const std::string tail = trim(arrayContent.substr(elementStart, arrayContent.size() - 1 - elementStart));
    if (!tail.empty()) {
        result.push_back(tail);
    }
    return result;
}

// --- Serialization ---

std::string SaveManager::serializeParticipant(const KingdomParticipantConfig& participant) {
    std::ostringstream ss;
    ss << "{ \"kingdom\": " << static_cast<int>(participant.kingdom)
       << ", \"controller\": " << static_cast<int>(participant.controller)
       << ", \"name\": \"" << escapeJsonString(participant.participantName) << "\" }";
    return ss.str();
}

std::string SaveManager::serializeMultiplayerConfig(const MultiplayerConfig& multiplayer) {
    std::ostringstream ss;
    ss << "{ \"enabled\": " << (multiplayer.enabled ? "true" : "false")
       << ", \"port\": " << multiplayer.port
       << ", \"passwordHash\": \"" << escapeJsonString(multiplayer.passwordHash) << "\""
       << ", \"passwordSalt\": \"" << escapeJsonString(multiplayer.passwordSalt) << "\""
       << ", \"protocolVersion\": " << multiplayer.protocolVersion
       << " }";
    return ss.str();
}

std::string SaveManager::serializePiece(const Piece& p) {
    std::ostringstream ss;
    ss << "{ \"id\": " << p.id
       << ", \"type\": " << static_cast<int>(p.type)
       << ", \"kingdom\": " << static_cast<int>(p.kingdom)
       << ", \"x\": " << p.position.x
       << ", \"y\": " << p.position.y
       << ", \"xp\": " << p.xp
       << ", \"formationId\": " << p.formationId
       << " }";
    return ss.str();
}

std::string SaveManager::serializeBuilding(const Building& b) {
    std::ostringstream ss;
    ss << "{ \"id\": " << b.id
       << ", \"type\": " << static_cast<int>(b.type)
       << ", \"owner\": " << static_cast<int>(b.owner)
       << ", \"isNeutral\": " << (b.isNeutral ? "true" : "false")
       << ", \"ox\": " << b.origin.x
       << ", \"oy\": " << b.origin.y
       << ", \"w\": " << b.width
       << ", \"h\": " << b.height
    << ", \"rot\": " << b.rotationQuarterTurns
    << ", \"fm\": " << b.flipMask
         << ", \"state\": " << static_cast<int>(b.state)
       << ", \"isProducing\": " << (b.isProducing ? "true" : "false")
       << ", \"producingType\": " << b.producingType
       << ", \"turnsRemaining\": " << b.turnsRemaining
       << ", \"hp\": [";
    for (std::size_t i = 0; i < b.cellHP.size(); ++i) {
        if (i > 0) ss << ",";
        ss << b.cellHP[i];
    }
    ss << "]"
       << ", \"breach\": [";
    for (std::size_t i = 0; i < b.cellBreachState.size(); ++i) {
        if (i > 0) ss << ",";
        ss << b.cellBreachState[i];
    }
    ss << "] }";
    return ss.str();
}

std::string SaveManager::serializeMapObject(const MapObject& object) {
    std::ostringstream ss;
    ss << "{ \"id\": " << object.id
       << ", \"type\": " << static_cast<int>(object.type)
       << ", \"x\": " << object.position.x
       << ", \"y\": " << object.position.y
       << ", \"rewardType\": " << static_cast<int>(object.chest.reward.type)
       << ", \"rewardAmount\": " << object.chest.reward.amount
       << ", \"spawnTurn\": " << object.chest.spawnTurn
       << " }";
    return ss.str();
}

std::string SaveManager::serializeAutonomousUnit(const AutonomousUnit& unit) {
    std::ostringstream ss;
    ss << "{ \"id\": " << unit.id
       << ", \"type\": " << static_cast<int>(unit.type)
       << ", \"x\": " << unit.position.x
       << ", \"y\": " << unit.position.y
       << ", \"targetKingdom\": " << static_cast<int>(unit.infernal.targetKingdom)
       << ", \"targetPieceId\": " << unit.infernal.targetPieceId
    << ", \"manifestedPieceType\": " << static_cast<int>(unit.infernal.manifestedPieceType)
       << ", \"preferredTargetType\": " << static_cast<int>(unit.infernal.preferredTargetType)
       << ", \"phase\": " << static_cast<int>(unit.infernal.phase)
       << ", \"returnX\": " << unit.infernal.returnBorderCell.x
       << ", \"returnY\": " << unit.infernal.returnBorderCell.y
       << ", \"spawnTurn\": " << unit.infernal.spawnTurn
       << " }";
    return ss.str();
}

std::string SaveManager::serializeEvent(const EventLog::Event& e) {
    std::ostringstream ss;
    ss << "{ \"turn\": " << e.turnNumber
       << ", \"kingdom\": " << static_cast<int>(e.kingdom)
       << ", \"msg\": \"" << escapeJsonString(e.message) << "\" }";
    return ss.str();
}

EventLog::Event SaveManager::parseEvent(const std::string& json) {
    EventLog::Event event;
    event.turnNumber = extractInt(json, "turn", 1);
    event.kingdom = static_cast<KingdomId>(extractInt(json, "kingdom", 0));
    event.message = extractString(json, "msg");
    return event;
}

KingdomParticipantConfig SaveManager::parseParticipant(const std::string& json) {
    KingdomParticipantConfig participant;
    participant.kingdom = static_cast<KingdomId>(extractInt(json, "kingdom", 0));
    participant.controller = static_cast<ControllerType>(extractInt(json, "controller", 0));
    participant.participantName = extractString(json, "name");
    return participant;
}

MultiplayerConfig SaveManager::parseMultiplayerConfig(const std::string& json) {
    MultiplayerConfig multiplayer;
    multiplayer.enabled = extractBool(json, "enabled", false);
    multiplayer.port = extractInt(json, "port", 0);
    multiplayer.passwordHash = extractString(json, "passwordHash");
    multiplayer.passwordSalt = extractString(json, "passwordSalt");

    const int protocolVersion = extractInt(
        json,
        "protocolVersion",
        static_cast<int>(kCurrentMultiplayerProtocolVersion));
    multiplayer.protocolVersion = (protocolVersion > 0)
        ? static_cast<std::uint32_t>(protocolVersion)
        : 0u;
    return multiplayer;
}

Piece SaveManager::parsePiece(const std::string& json) {
    Piece p;
    p.id = extractInt(json, "id", 0);
    p.type = static_cast<PieceType>(extractInt(json, "type", 0));
    p.kingdom = static_cast<KingdomId>(extractInt(json, "kingdom", 0));
    p.position.x = extractInt(json, "x", 0);
    p.position.y = extractInt(json, "y", 0);
    p.xp = extractInt(json, "xp", 0);
    p.formationId = extractInt(json, "formationId", -1);
    return p;
}

Building SaveManager::parseBuilding(const std::string& json) {
    Building b;
    b.id = extractInt(json, "id", 0);
    b.type = static_cast<BuildingType>(extractInt(json, "type", 0));
    b.owner = static_cast<KingdomId>(extractInt(json, "owner", 0));
    b.isNeutral = extractBool(json, "isNeutral", false);
    b.origin.x = extractInt(json, "ox", 0);
    b.origin.y = extractInt(json, "oy", 0);
    b.width = extractInt(json, "w", 1);
    b.height = extractInt(json, "h", 1);
    b.rotationQuarterTurns = extractInt(json, "rot", -1);
    b.flipMask = extractInt(json, "fm", -1);
    const int rawState = extractInt(json, "state", static_cast<int>(BuildingState::Completed));
    b.state = (rawState == static_cast<int>(BuildingState::UnderConstruction))
        ? BuildingState::UnderConstruction
        : BuildingState::Completed;
    b.isProducing = extractBool(json, "isProducing", false);
    b.producingType = extractInt(json, "producingType", 0);
    b.turnsRemaining = extractInt(json, "turnsRemaining", 0);

    // Parse HP array
    std::string hpArr = extractArray(json, "hp");
    b.cellHP.clear();
    std::size_t pos = 1; // skip '['
    while (pos < hpArr.size()) {
        while (pos < hpArr.size() && (hpArr[pos] == ' ' || hpArr[pos] == ',')) pos++;
        if (pos >= hpArr.size() || hpArr[pos] == ']') break;
        try {
            int val = std::stoi(hpArr.substr(pos));
            b.cellHP.push_back(val);
        } catch (...) { break; }
        while (pos < hpArr.size() && hpArr[pos] != ',' && hpArr[pos] != ']') pos++;
    }

    std::string breachArr = extractArray(json, "breach");
    b.cellBreachState.clear();
    pos = 1;
    while (pos < breachArr.size()) {
        while (pos < breachArr.size() && (breachArr[pos] == ' ' || breachArr[pos] == ',')) pos++;
        if (pos >= breachArr.size() || breachArr[pos] == ']') break;
        try {
            int val = std::stoi(breachArr.substr(pos));
            b.cellBreachState.push_back(val != 0 ? 1 : 0);
        } catch (...) {
            break;
        }
        while (pos < breachArr.size() && breachArr[pos] != ',' && breachArr[pos] != ']') pos++;
    }

    if (b.cellBreachState.size() < b.cellHP.size()) {
        b.cellBreachState.resize(b.cellHP.size(), 0);
    }

    return b;
}

MapObject SaveManager::parseMapObject(const std::string& json) {
    MapObject object;
    object.id = extractInt(json, "id", -1);
    object.type = static_cast<MapObjectType>(extractInt(json, "type", 0));
    object.position.x = extractInt(json, "x", 0);
    object.position.y = extractInt(json, "y", 0);
    object.chest.reward.type = static_cast<ChestRewardType>(extractInt(json, "rewardType", 0));
    object.chest.reward.amount = extractInt(json, "rewardAmount", 0);
    object.chest.spawnTurn = extractInt(json, "spawnTurn", 0);
    return object;
}

AutonomousUnit SaveManager::parseAutonomousUnit(const std::string& json) {
    AutonomousUnit unit;
    unit.id = extractInt(json, "id", -1);
    unit.type = static_cast<AutonomousUnitType>(extractInt(json, "type", 0));
    unit.position.x = extractInt(json, "x", 0);
    unit.position.y = extractInt(json, "y", 0);
    unit.infernal.targetKingdom = static_cast<KingdomId>(extractInt(json, "targetKingdom", 0));
    unit.infernal.targetPieceId = extractInt(json, "targetPieceId", -1);
    unit.infernal.manifestedPieceType = static_cast<PieceType>(extractInt(
        json,
        "manifestedPieceType",
        static_cast<int>(PieceType::Queen)));
    unit.infernal.preferredTargetType = static_cast<PieceType>(extractInt(
        json,
        "preferredTargetType",
        static_cast<int>(PieceType::Queen)));
    unit.infernal.phase = static_cast<InfernalPhase>(extractInt(json, "phase", 0));
    unit.infernal.returnBorderCell.x = extractInt(json, "returnX", -1);
    unit.infernal.returnBorderCell.y = extractInt(json, "returnY", -1);
    unit.infernal.spawnTurn = extractInt(json, "spawnTurn", 0);
    return unit;
}

// --- Save / Load ---

bool SaveManager::save(const std::string& filepath, const SaveData& data) {
    fs::path target(filepath);
    if (target.has_parent_path()) {
        std::error_code mkdirError;
        fs::create_directories(target.parent_path(), mkdirError);
    }

    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    writeJson(file, data);
    file.close();
    return true;
}

bool SaveManager::load(const std::string& filepath, SaveData& outData) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    return deserialize(ss.str(), outData);
}

std::string SaveManager::serialize(const SaveData& data) {
    std::ostringstream output;
    writeJson(output, data);
    return output.str();
}

bool SaveManager::deserialize(const std::string& json, SaveData& outData) {
    if (json.empty()) {
        return false;
    }

    outData.gameName = extractString(json, "gameName");
    outData.turnNumber = extractInt(json, "turnNumber", 1);
    outData.activeKingdom = static_cast<KingdomId>(extractInt(json, "activeKingdom", 0));
    outData.mapRadius = extractInt(json, "mapRadius", 50);
    outData.worldSeed = static_cast<std::uint32_t>(std::max(0, extractInt(json, "worldSeed", 0)));
    outData.sessionKingdoms = defaultKingdomParticipants(GameMode::HumanVsHuman);
    const std::string participantsArray = extractArray(json, "sessionKingdoms");
    const auto participantElements = splitArrayElements(participantsArray);
    if (participantElements.size() == kNumKingdoms) {
        for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
            outData.sessionKingdoms[kingdomSlot] = parseParticipant(participantElements[kingdomSlot]);
        }
    } else {
        const auto defaultNames = defaultParticipantNames(GameMode::HumanVsHuman);
        const std::string whiteName = extractString(json, "whiteName");
        const std::string blackName = extractString(json, "blackName");
        outData.sessionKingdoms[kingdomIndex(KingdomId::White)].participantName =
            whiteName.empty() ? defaultNames[0] : whiteName;
        outData.sessionKingdoms[kingdomIndex(KingdomId::Black)].participantName =
            blackName.empty() ? defaultNames[1] : blackName;
    }
    outData.multiplayer = parseMultiplayerConfig(extractSection(json, "multiplayer"));

    // Parse kingdoms
    // Parse kingdoms (JSON keys kept for backward compatibility)
    static const char* kingdomKeys[] = {"whiteKingdom", "blackKingdom"};
    for (int k = 0; k < kNumKingdoms; ++k) {
        KingdomId id = static_cast<KingdomId>(k);
        std::string section = extractSection(json, kingdomKeys[k]);
        outData.kingdoms[k].id = id;
        outData.kingdoms[k].gold = extractInt(section, "gold", 0);
        outData.kingdoms[k].movementPointsMaxBonus = extractInt(section, "movementPointsMaxBonus", 0);
        outData.kingdoms[k].buildPointsMaxBonus = extractInt(section, "buildPointsMaxBonus", 0);
        outData.kingdoms[k].hasSpawnedBishop = extractInt(section, "hasSpawnedBishop", 0) != 0;
        outData.kingdoms[k].lastBishopSpawnParity = extractInt(section, "lastBishopSpawnParity", 0) & 1;

        std::string piecesArr = extractArray(section, "pieces");
        auto pieceElems = splitArrayElements(piecesArr);
        outData.kingdoms[k].pieces.clear();
        for (const auto& elem : pieceElems)
            outData.kingdoms[k].pieces.push_back(parsePiece(elem));

        std::string buildingsArr = extractArray(section, "buildings");
        auto buildingElems = splitArrayElements(buildingsArr);
        outData.kingdoms[k].buildings.clear();
        for (const auto& elem : buildingElems)
            outData.kingdoms[k].buildings.push_back(parseBuilding(elem));
    }

    // Public buildings
    std::string pubArr = extractArray(json, "publicBuildings");
    auto pubElements = splitArrayElements(pubArr);
    outData.publicBuildings.clear();
    for (const auto& elem : pubElements)
        outData.publicBuildings.push_back(parseBuilding(elem));

    std::string mapObjectsArray = extractArray(json, "mapObjects");
    outData.mapObjects.clear();
    for (const auto& elem : splitArrayElements(mapObjectsArray)) {
        outData.mapObjects.push_back(parseMapObject(elem));
    }

    std::string autonomousUnitsArray = extractArray(json, "autonomousUnits");
    outData.autonomousUnits.clear();
    for (const auto& elem : splitArrayElements(autonomousUnitsArray)) {
        outData.autonomousUnits.push_back(parseAutonomousUnit(elem));
    }

    const std::string chestStateSection = extractSection(json, "chestState");
    outData.chestSystemState.activeChestObjectId = extractInt(
        chestStateSection, "activeChestObjectId", -1);
    outData.chestSystemState.nextSpawnTurn = extractInt(chestStateSection, "nextSpawnTurn", 0);
    outData.chestSystemState.rngCounter = static_cast<std::uint32_t>(std::max(
        0,
        extractInt(chestStateSection, "rngCounter", 0)));

    const std::string weatherStateSection = extractSection(json, "weatherState");
    outData.weatherSystemState.nextSpawnTurnStep = extractInt(
        weatherStateSection, "nextSpawnTurnStep", 0);
    outData.weatherSystemState.hasActiveFront = extractInt(
        weatherStateSection, "hasActiveFront", 0) != 0;
    outData.weatherSystemState.rngCounter = static_cast<std::uint32_t>(std::max(
        0,
        extractInt(weatherStateSection, "rngCounter", 0)));
    outData.weatherSystemState.revision = static_cast<std::uint32_t>(std::max(
        0,
        extractInt(weatherStateSection, "revision", 0)));
    const std::string weatherFrontSection = extractSection(weatherStateSection, "activeFront");
    outData.weatherSystemState.activeFront.direction = static_cast<WeatherDirection>(std::clamp(
        extractInt(weatherFrontSection, "direction", static_cast<int>(WeatherDirection::East)),
        0,
        static_cast<int>(WeatherDirection::Count) - 1));
    outData.weatherSystemState.activeFront.currentTurnStep = extractInt(
        weatherFrontSection, "currentTurnStep", 0);
    outData.weatherSystemState.activeFront.totalTurnSteps = extractInt(
        weatherFrontSection, "totalTurnSteps", 0);
    outData.weatherSystemState.activeFront.centerStartXTimes1000 = extractInt(
        weatherFrontSection, "centerStartXTimes1000", 0);
    outData.weatherSystemState.activeFront.centerStartYTimes1000 = extractInt(
        weatherFrontSection, "centerStartYTimes1000", 0);
    outData.weatherSystemState.activeFront.stepXTimes1000 = extractInt(
        weatherFrontSection, "stepXTimes1000", 0);
    outData.weatherSystemState.activeFront.stepYTimes1000 = extractInt(
        weatherFrontSection, "stepYTimes1000", 0);
    outData.weatherSystemState.activeFront.radiusAlongTimes1000 = extractInt(
        weatherFrontSection, "radiusAlongTimes1000", 0);
    outData.weatherSystemState.activeFront.radiusAcrossTimes1000 = extractInt(
        weatherFrontSection, "radiusAcrossTimes1000", 0);
    outData.weatherSystemState.activeFront.shapeSeed = static_cast<std::uint32_t>(std::max(
        0,
        extractInt(weatherFrontSection, "shapeSeed", 0)));
    outData.weatherSystemState.activeFront.densitySeed = static_cast<std::uint32_t>(std::max(
        0,
        extractInt(weatherFrontSection, "densitySeed", 0)));
    const std::string weatherMaskSection = extractSection(weatherStateSection, "mask");
    outData.weatherMaskCache.revision = static_cast<std::uint32_t>(std::max(
        0,
        extractInt(weatherMaskSection, "revision", 0)));
    outData.weatherMaskCache.diameter = std::max(0, extractInt(weatherMaskSection, "diameter", 0));
    outData.weatherMaskCache.hasActiveFront = extractInt(weatherMaskSection, "hasActiveFront", 0) != 0;
    outData.weatherMaskCache.alphaByCell = parseUInt8Array(extractArray(weatherMaskSection, "alphaByCell"));
    outData.weatherMaskCache.shadeByCell = parseUInt8Array(extractArray(weatherMaskSection, "shadeByCell"));

    const std::string xpStateSection = extractSection(json, "xpState");
    outData.xpSystemState.rngCounter = static_cast<std::uint32_t>(std::max(
        0,
        extractInt(xpStateSection, "rngCounter", 0)));

    const std::string infernalStateSection = extractSection(json, "infernalState");
    outData.infernalSystemState.activeInfernalUnitId = extractInt(
        infernalStateSection, "activeInfernalUnitId", -1);
    outData.infernalSystemState.nextSpawnTurn = extractInt(infernalStateSection, "nextSpawnTurn", 0);
    outData.infernalSystemState.whiteBloodDebt = extractInt(infernalStateSection, "whiteBloodDebt", 0);
    outData.infernalSystemState.blackBloodDebt = extractInt(infernalStateSection, "blackBloodDebt", 0);
    outData.infernalSystemState.rngCounter = static_cast<std::uint32_t>(std::max(
        0,
        extractInt(infernalStateSection, "rngCounter", 0)));

    std::string gridArray = extractArray(json, "grid");
    outData.grid.clear();
    for (const auto& rowElement : splitArrayElements(gridArray)) {
        std::vector<SaveData::CellData> row;
        for (const auto& cellElement : splitArrayElements(rowElement)) {
            SaveData::CellData cell;
            cell.type = static_cast<CellType>(extractInt(cellElement, "t", static_cast<int>(CellType::Grass)));
            cell.isInCircle = extractInt(cellElement, "c", 0) != 0;
            cell.terrainFlipMask = extractInt(cellElement, "f", -1);
            cell.terrainBrightness = static_cast<std::uint8_t>(std::clamp(extractInt(cellElement, "b", 255), 0, 255));
            row.push_back(cell);
        }
        outData.grid.push_back(std::move(row));
    }

    std::string eventsArray = extractArray(json, "events");
    outData.events.clear();
    for (const auto& elem : splitArrayElements(eventsArray)) {
        outData.events.push_back(parseEvent(elem));
    }

    return true;
}

std::vector<std::string> SaveManager::listSaves(const std::string& savesDir) {
    std::vector<std::string> result;
    for (const auto& save : listSaveSummaries(savesDir)) {
        result.push_back(save.saveName);
    }
    return result;
}

std::vector<SaveSummary> SaveManager::listSaveSummaries(const std::string& savesDir) {
    std::vector<std::pair<fs::file_time_type, SaveSummary>> entries;
    if (!fs::exists(savesDir)) return {};

    for (const auto& entry : fs::directory_iterator(savesDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }

        SaveSummary summary;
        summary.saveName = entry.path().stem().string();

        SaveData data;
        if (load(entry.path().string(), data)) {
            summary.kingdoms = data.sessionKingdoms;
            summary.multiplayer = data.multiplayer;
        }

        std::error_code timeError;
        fs::file_time_type lastWriteTime = fs::last_write_time(entry.path(), timeError);
        if (timeError) {
            lastWriteTime = fs::file_time_type::min();
        }

        entries.push_back({lastWriteTime, summary});
    }

    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first > rhs.first;
    });

    std::vector<SaveSummary> result;
    result.reserve(entries.size());
    for (const auto& entry : entries) {
        result.push_back(entry.second);
    }
    return result;
}

bool SaveManager::deleteSave(const std::string& filepath) {
    return fs::remove(filepath);
}
