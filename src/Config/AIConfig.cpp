#include "Config/AIConfig.hpp"
#include <iostream>

AIConfig::AIConfig()
    : farmPriority(0.8f), attackPriority(0.6f), defensePriority(0.7f),
      buildPriority(0.5f), upgradePriority(0.4f), marriagePriority(0.3f),
      minGoldBeforeAttack(100), minPiecesBeforeAttack(3), wallDefenseRadius(5),
      randomness(0.1f) {}

std::string AIConfig::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string AIConfig::extractSection(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find('{', pos);
    if (pos == std::string::npos) return "";
    int depth = 1;
    size_t start = pos;
    ++pos;
    while (pos < json.size() && depth > 0) {
        if (json[pos] == '{') ++depth;
        if (json[pos] == '}') --depth;
        ++pos;
    }
    return json.substr(start, pos - start);
}

float AIConfig::extractFloat(const std::string& json, const std::string& key, float defaultVal) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    try {
        size_t end;
        float val = std::stof(json.substr(pos), &end);
        return val;
    } catch (...) {
        return defaultVal;
    }
}

int AIConfig::extractInt(const std::string& json, const std::string& key, int defaultVal) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    try {
        return std::stoi(json.substr(pos));
    } catch (...) {
        return defaultVal;
    }
}

bool AIConfig::loadFromFile(const std::string& filepath) {
    std::string json = readFile(filepath);
    if (json.empty()) {
        std::cerr << "AIConfig: Could not load " << filepath << ", using defaults.\n";
        return false;
    }

    std::string weightsSec = extractSection(json, "weights");
    if (!weightsSec.empty()) {
        farmPriority = extractFloat(weightsSec, "farm_priority", farmPriority);
        attackPriority = extractFloat(weightsSec, "attack_priority", attackPriority);
        defensePriority = extractFloat(weightsSec, "defense_priority", defensePriority);
        buildPriority = extractFloat(weightsSec, "build_priority", buildPriority);
        upgradePriority = extractFloat(weightsSec, "upgrade_priority", upgradePriority);
        marriagePriority = extractFloat(weightsSec, "marriage_priority", marriagePriority);
    }

    std::string thresholdsSec = extractSection(json, "thresholds");
    if (!thresholdsSec.empty()) {
        minGoldBeforeAttack = extractInt(thresholdsSec, "min_gold_before_attack", minGoldBeforeAttack);
        minPiecesBeforeAttack = extractInt(thresholdsSec, "min_pieces_before_attack", minPiecesBeforeAttack);
        wallDefenseRadius = extractInt(thresholdsSec, "wall_defense_radius", wallDefenseRadius);
    }

    randomness = extractFloat(json, "randomness", randomness);
    return true;
}
