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
       << ", \"isProducing\": " << (b.isProducing ? "true" : "false")
       << ", \"producingType\": " << b.producingType
       << ", \"turnsRemaining\": " << b.turnsRemaining
       << ", \"hp\": [";
    for (std::size_t i = 0; i < b.cellHP.size(); ++i) {
        if (i > 0) ss << ",";
        ss << b.cellHP[i];
    }
    ss << "] }";
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

    return b;
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

    SaveData normalized = data;
    normalized.refreshLegacyMetadataFromSession();

    file << "{\n";
    file << "  \"gameName\": \"" << escapeJsonString(normalized.gameName) << "\",\n";
    file << "  \"turnNumber\": " << normalized.turnNumber << ",\n";
    file << "  \"activeKingdom\": " << static_cast<int>(normalized.activeKingdom) << ",\n";
    file << "  \"mapRadius\": " << normalized.mapRadius << ",\n";
    file << "  \"gameMode\": " << static_cast<int>(normalized.mode) << ",\n";
    file << "  \"whiteController\": " << static_cast<int>(normalized.controllers[0]) << ",\n";
    file << "  \"blackController\": " << static_cast<int>(normalized.controllers[1]) << ",\n";
    file << "  \"whiteName\": \"" << escapeJsonString(normalized.participantNames[0]) << "\",\n";
    file << "  \"blackName\": \"" << escapeJsonString(normalized.participantNames[1]) << "\",\n";
    file << "  \"sessionKingdoms\": [";
    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        if (kingdomSlot > 0) file << ", ";
        file << serializeParticipant(normalized.sessionKingdoms[kingdomSlot]);
    }
    file << "],\n";

    // Grid
    file << "  \"grid\": [\n";
    for (std::size_t y = 0; y < normalized.grid.size(); ++y) {
        file << "    [";
        for (std::size_t x = 0; x < normalized.grid[y].size(); ++x) {
            file << "{\"t\":" << static_cast<int>(normalized.grid[y][x].type)
                 << ",\"c\":" << (normalized.grid[y][x].isInCircle ? 1 : 0) << "}";
            if (x + 1 < normalized.grid[y].size()) file << ",";
        }
        file << "]";
        if (y + 1 < normalized.grid.size()) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // Kingdoms (JSON keys "whiteKingdom"/"blackKingdom" kept for backward compatibility)
    static const char* kingdomKeys[] = {"whiteKingdom", "blackKingdom"};
    for (int k = 0; k < kNumKingdoms; ++k) {
        const auto& kd = normalized.kingdoms[k];
        file << "  \"" << kingdomKeys[k] << "\": {\n";
        file << "    \"gold\": " << kd.gold << ",\n";
        file << "    \"pieces\": [";
        for (std::size_t i = 0; i < kd.pieces.size(); ++i) {
            if (i > 0) file << ", ";
            file << serializePiece(kd.pieces[i]);
        }
        file << "],\n";
        file << "    \"buildings\": [";
        for (std::size_t i = 0; i < kd.buildings.size(); ++i) {
            if (i > 0) file << ", ";
            file << serializeBuilding(kd.buildings[i]);
        }
        file << "]\n  }";
        file << ",\n";
    }

    // Public buildings
    file << "  \"publicBuildings\": [";
    for (std::size_t i = 0; i < normalized.publicBuildings.size(); ++i) {
        if (i > 0) file << ", ";
        file << serializeBuilding(normalized.publicBuildings[i]);
    }
    file << "],\n";

    // Events
    file << "  \"events\": [";
    for (std::size_t i = 0; i < normalized.events.size(); ++i) {
        if (i > 0) file << ", ";
        file << serializeEvent(normalized.events[i]);
    }
    file << "]\n";

    file << "}\n";
    file.close();
    return true;
}

bool SaveManager::load(const std::string& filepath, SaveData& outData) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::stringstream ss;
    ss << file.rdbuf();
    std::string json = ss.str();
    file.close();

    outData.gameName = extractString(json, "gameName");
    outData.turnNumber = extractInt(json, "turnNumber", 1);
    outData.activeKingdom = static_cast<KingdomId>(extractInt(json, "activeKingdom", 0));
    outData.mapRadius = extractInt(json, "mapRadius", 50);
    outData.mode = static_cast<GameMode>(extractInt(json, "gameMode", static_cast<int>(outData.mode)));

    const auto defaultControllers = controllersForGameMode(outData.mode);
    outData.controllers[0] = static_cast<ControllerType>(
        extractInt(json, "whiteController", static_cast<int>(defaultControllers[0])));
    outData.controllers[1] = static_cast<ControllerType>(
        extractInt(json, "blackController", static_cast<int>(defaultControllers[1])));

    const auto defaultNames = defaultParticipantNames(outData.mode);
    outData.participantNames[0] = extractString(json, "whiteName");
    outData.participantNames[1] = extractString(json, "blackName");
    if (outData.participantNames[0].empty()) outData.participantNames[0] = defaultNames[0];
    if (outData.participantNames[1].empty()) outData.participantNames[1] = defaultNames[1];

    outData.refreshSessionFromLegacyMetadata();
    const std::string participantsArray = extractArray(json, "sessionKingdoms");
    const auto participantElements = splitArrayElements(participantsArray);
    if (participantElements.size() == kNumKingdoms) {
        for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
            outData.sessionKingdoms[kingdomSlot] = parseParticipant(participantElements[kingdomSlot]);
        }
    }
    outData.refreshLegacyMetadataFromSession();

    // Parse kingdoms
    // Parse kingdoms (JSON keys kept for backward compatibility)
    static const char* kingdomKeys[] = {"whiteKingdom", "blackKingdom"};
    for (int k = 0; k < kNumKingdoms; ++k) {
        KingdomId id = static_cast<KingdomId>(k);
        std::string section = extractSection(json, kingdomKeys[k]);
        outData.kingdoms[k].id = id;
        outData.kingdoms[k].gold = extractInt(section, "gold", 0);

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

    std::string gridArray = extractArray(json, "grid");
    outData.grid.clear();
    for (const auto& rowElement : splitArrayElements(gridArray)) {
        std::vector<SaveData::CellData> row;
        for (const auto& cellElement : splitArrayElements(rowElement)) {
            SaveData::CellData cell;
            cell.type = static_cast<CellType>(extractInt(cellElement, "t", static_cast<int>(CellType::Grass)));
            cell.isInCircle = extractInt(cellElement, "c", 0) != 0;
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
