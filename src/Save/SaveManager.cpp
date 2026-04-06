#include "Save/SaveManager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// --- Simple JSON helpers ---

std::string SaveManager::extractString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    std::size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    std::size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

int SaveManager::extractInt(const std::string& json, const std::string& key, int defaultVal) {
    std::string search = "\"" + key + "\"";
    std::size_t pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    try {
        return std::stoi(json.substr(pos));
    } catch (...) {
        return defaultVal;
    }
}

std::string SaveManager::extractSection(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    std::size_t pos = json.find(search);
    if (pos == std::string::npos) return "{}";
    pos = json.find('{', pos);
    if (pos == std::string::npos) return "{}";
    int depth = 1;
    std::size_t start = pos;
    pos++;
    while (pos < json.size() && depth > 0) {
        if (json[pos] == '{') depth++;
        else if (json[pos] == '}') depth--;
        pos++;
    }
    return json.substr(start, pos - start);
}

std::string SaveManager::extractArray(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    std::size_t pos = json.find(search);
    if (pos == std::string::npos) return "[]";
    pos = json.find('[', pos);
    if (pos == std::string::npos) return "[]";
    int depth = 1;
    std::size_t start = pos;
    pos++;
    while (pos < json.size() && depth > 0) {
        if (json[pos] == '[') depth++;
        else if (json[pos] == ']') depth--;
        pos++;
    }
    return json.substr(start, pos - start);
}

std::vector<std::string> SaveManager::splitArrayElements(const std::string& arrayContent) {
    std::vector<std::string> result;
    // Find elements between { }
    std::size_t pos = 0;
    while (pos < arrayContent.size()) {
        std::size_t start = arrayContent.find('{', pos);
        if (start == std::string::npos) break;
        int depth = 1;
        std::size_t end = start + 1;
        while (end < arrayContent.size() && depth > 0) {
            if (arrayContent[end] == '{') depth++;
            else if (arrayContent[end] == '}') depth--;
            end++;
        }
        result.push_back(arrayContent.substr(start, end - start));
        pos = end;
    }
    return result;
}

// --- Serialization ---

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
       << ", \"msg\": \"" << e.message << "\" }";
    return ss.str();
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
    // isNeutral
    b.isNeutral = (json.find("\"isNeutral\": true") != std::string::npos) ||
                  (json.find("\"isNeutral\":true") != std::string::npos);
    b.origin.x = extractInt(json, "ox", 0);
    b.origin.y = extractInt(json, "oy", 0);
    b.width = extractInt(json, "w", 1);
    b.height = extractInt(json, "h", 1);
    b.isProducing = (json.find("\"isProducing\": true") != std::string::npos) ||
                    (json.find("\"isProducing\":true") != std::string::npos);
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
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"gameName\": \"" << data.gameName << "\",\n";
    file << "  \"turnNumber\": " << data.turnNumber << ",\n";
    file << "  \"activeKingdom\": " << static_cast<int>(data.activeKingdom) << ",\n";
    file << "  \"mapRadius\": " << data.mapRadius << ",\n";

    // Grid
    file << "  \"grid\": [\n";
    for (std::size_t y = 0; y < data.grid.size(); ++y) {
        file << "    [";
        for (std::size_t x = 0; x < data.grid[y].size(); ++x) {
            file << "{\"t\":" << static_cast<int>(data.grid[y][x].type)
                 << ",\"c\":" << (data.grid[y][x].isInCircle ? 1 : 0) << "}";
            if (x + 1 < data.grid[y].size()) file << ",";
        }
        file << "]";
        if (y + 1 < data.grid.size()) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // White kingdom
    file << "  \"whiteKingdom\": {\n";
    file << "    \"gold\": " << data.whiteKingdom.gold << ",\n";
    file << "    \"pieces\": [";
    for (std::size_t i = 0; i < data.whiteKingdom.pieces.size(); ++i) {
        if (i > 0) file << ", ";
        file << serializePiece(data.whiteKingdom.pieces[i]);
    }
    file << "],\n";
    file << "    \"buildings\": [";
    for (std::size_t i = 0; i < data.whiteKingdom.buildings.size(); ++i) {
        if (i > 0) file << ", ";
        file << serializeBuilding(data.whiteKingdom.buildings[i]);
    }
    file << "]\n  },\n";

    // Black kingdom
    file << "  \"blackKingdom\": {\n";
    file << "    \"gold\": " << data.blackKingdom.gold << ",\n";
    file << "    \"pieces\": [";
    for (std::size_t i = 0; i < data.blackKingdom.pieces.size(); ++i) {
        if (i > 0) file << ", ";
        file << serializePiece(data.blackKingdom.pieces[i]);
    }
    file << "],\n";
    file << "    \"buildings\": [";
    for (std::size_t i = 0; i < data.blackKingdom.buildings.size(); ++i) {
        if (i > 0) file << ", ";
        file << serializeBuilding(data.blackKingdom.buildings[i]);
    }
    file << "]\n  },\n";

    // Public buildings
    file << "  \"publicBuildings\": [";
    for (std::size_t i = 0; i < data.publicBuildings.size(); ++i) {
        if (i > 0) file << ", ";
        file << serializeBuilding(data.publicBuildings[i]);
    }
    file << "],\n";

    // Events
    file << "  \"events\": [";
    for (std::size_t i = 0; i < data.events.size(); ++i) {
        if (i > 0) file << ", ";
        file << serializeEvent(data.events[i]);
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

    // Parse kingdoms
    std::string whiteSection = extractSection(json, "whiteKingdom");
    outData.whiteKingdom.id = KingdomId::White;
    outData.whiteKingdom.gold = extractInt(whiteSection, "gold", 0);

    std::string whitePieces = extractArray(whiteSection, "pieces");
    auto wpElements = splitArrayElements(whitePieces);
    outData.whiteKingdom.pieces.clear();
    for (const auto& elem : wpElements)
        outData.whiteKingdom.pieces.push_back(parsePiece(elem));

    std::string whiteBuildings = extractArray(whiteSection, "buildings");
    auto wbElements = splitArrayElements(whiteBuildings);
    outData.whiteKingdom.buildings.clear();
    for (const auto& elem : wbElements)
        outData.whiteKingdom.buildings.push_back(parseBuilding(elem));

    std::string blackSection = extractSection(json, "blackKingdom");
    outData.blackKingdom.id = KingdomId::Black;
    outData.blackKingdom.gold = extractInt(blackSection, "gold", 0);

    std::string blackPieces = extractArray(blackSection, "pieces");
    auto bpElements = splitArrayElements(blackPieces);
    outData.blackKingdom.pieces.clear();
    for (const auto& elem : bpElements)
        outData.blackKingdom.pieces.push_back(parsePiece(elem));

    std::string blackBuildings = extractArray(blackSection, "buildings");
    auto bbElements = splitArrayElements(blackBuildings);
    outData.blackKingdom.buildings.clear();
    for (const auto& elem : bbElements)
        outData.blackKingdom.buildings.push_back(parseBuilding(elem));

    // Public buildings
    std::string pubArr = extractArray(json, "publicBuildings");
    auto pubElements = splitArrayElements(pubArr);
    outData.publicBuildings.clear();
    for (const auto& elem : pubElements)
        outData.publicBuildings.push_back(parseBuilding(elem));

    // Grid loading is heavy for large maps — skip for now if not present
    // (Board will be regenerated from pieces/buildings state for save compatibility)
    outData.grid.clear();

    return true;
}

std::vector<std::string> SaveManager::listSaves(const std::string& savesDir) {
    std::vector<std::string> result;
    if (!fs::exists(savesDir)) return result;
    for (const auto& entry : fs::directory_iterator(savesDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            result.push_back(entry.path().stem().string());
        }
    }
    return result;
}

bool SaveManager::deleteSave(const std::string& filepath) {
    return fs::remove(filepath);
}
