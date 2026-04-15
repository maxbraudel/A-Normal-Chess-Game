#pragma once
#include <iosfwd>
#include <string>
#include <vector>
#include "Save/SaveData.hpp"

class SaveManager {
public:
    bool save(const std::string& filepath, const SaveData& data);
    bool load(const std::string& filepath, SaveData& outData);
    std::string serialize(const SaveData& data);
    bool deserialize(const std::string& text, SaveData& outData);
    std::vector<std::string> listSaves(const std::string& savesDir);
    std::vector<SaveSummary> listSaveSummaries(const std::string& savesDir);
    bool deleteSave(const std::string& filepath);

private:
    // Custom JSON serialization helpers
    static void writeJson(std::ostream& output, const SaveData& data);
    static std::string serializeParticipant(const KingdomParticipantConfig& participant);
    static std::string serializeMultiplayerConfig(const MultiplayerConfig& multiplayer);
    static std::string serializePiece(const Piece& p);
    static std::string serializeBuilding(const Building& b);
    static std::string serializeEvent(const EventLog::Event& e);
    static EventLog::Event parseEvent(const std::string& json);
    static KingdomParticipantConfig parseParticipant(const std::string& json);
    static MultiplayerConfig parseMultiplayerConfig(const std::string& json);
    static Piece parsePiece(const std::string& json);
    static Building parseBuilding(const std::string& json);

    static std::string escapeJsonString(const std::string& value);
    static std::string extractString(const std::string& json, const std::string& key);
    static int extractInt(const std::string& json, const std::string& key, int defaultVal);
    static bool extractBool(const std::string& json, const std::string& key, bool defaultVal);
    static std::string extractSection(const std::string& json, const std::string& key);
    static std::string extractArray(const std::string& json, const std::string& key);
    static std::vector<std::string> splitArrayElements(const std::string& arrayContent);
};
