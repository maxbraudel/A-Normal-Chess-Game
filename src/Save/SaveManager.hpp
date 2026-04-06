#pragma once
#include <string>
#include <vector>
#include "Save/SaveData.hpp"

class SaveManager {
public:
    bool save(const std::string& filepath, const SaveData& data);
    bool load(const std::string& filepath, SaveData& outData);
    std::vector<std::string> listSaves(const std::string& savesDir);
    bool deleteSave(const std::string& filepath);

private:
    // Custom JSON serialization helpers
    static std::string serializePiece(const Piece& p);
    static std::string serializeBuilding(const Building& b);
    static std::string serializeEvent(const EventLog::Event& e);
    static Piece parsePiece(const std::string& json);
    static Building parseBuilding(const std::string& json);

    static std::string extractString(const std::string& json, const std::string& key);
    static int extractInt(const std::string& json, const std::string& key, int defaultVal);
    static std::string extractSection(const std::string& json, const std::string& key);
    static std::string extractArray(const std::string& json, const std::string& key);
    static std::vector<std::string> splitArrayElements(const std::string& arrayContent);
};
