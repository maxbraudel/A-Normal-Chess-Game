#pragma once
#include <string>
#include <fstream>
#include <sstream>

class AIConfig {
public:
    AIConfig();
    bool loadFromFile(const std::string& filepath);

    // Weights
    float farmPriority;
    float attackPriority;
    float defensePriority;
    float buildPriority;
    float upgradePriority;
    float marriagePriority;

    // Thresholds
    int minGoldBeforeAttack;
    int minPiecesBeforeAttack;
    int wallDefenseRadius;

    float randomness;

private:
    static std::string readFile(const std::string& path);
    static float extractFloat(const std::string& json, const std::string& key, float defaultVal);
    static int extractInt(const std::string& json, const std::string& key, int defaultVal);
    static std::string extractSection(const std::string& json, const std::string& key);
};
