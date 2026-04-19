#include "Config/GameConfig.hpp"

#include "Buildings/StructureChunkRegistry.hpp"

#include <iostream>
#include <algorithm>
#include <cctype>
#include <optional>

namespace {

XPRewardProfile makeXPRewardProfile(int mean,
                                    int sigmaMultiplierTimes100,
                                    int clampSigmaMultiplierTimes100,
                                    int minimum) {
    XPRewardProfile profile;
    profile.mean = mean;
    profile.sigmaMultiplierTimes100 = sigmaMultiplierTimes100;
    profile.clampSigmaMultiplierTimes100 = clampSigmaMultiplierTimes100;
    profile.minimum = minimum;
    return profile;
}

void alignChunkedStructureDimensions(const char* label, BuildingType type, int& width, int& height) {
    const int expectedWidth = StructureChunkRegistry::getChunkWidth(type, width);
    const int expectedHeight = StructureChunkRegistry::getChunkHeight(type, height);
    if (width == expectedWidth && height == expectedHeight) {
        return;
    }

    std::cerr << "GameConfig: Overriding " << label << " size from "
              << width << "x" << height << " to "
              << expectedWidth << "x" << expectedHeight
              << " to match structure chunk textures.\n";

    width = expectedWidth;
    height = expectedHeight;
}

int clampNonNegativeConfigValue(const char* label, int value) {
    if (value >= 0) {
        return value;
    }

    std::cerr << "GameConfig: Clamping negative " << label << " value "
              << value << " to 0.\n";
    return 0;
}

int clampRangedConfigValue(const std::string& label, int value, int minValue, int maxValue) {
    const int clampedValue = std::clamp(value, minValue, maxValue);
    if (clampedValue == value) {
        return clampedValue;
    }

    std::cerr << "GameConfig: Clamping " << label << " value "
              << value << " to range [" << minValue << ", " << maxValue << "].\n";
    return clampedValue;
}

int clampPositiveConfigValue(const std::string& label, int value) {
    return clampRangedConfigValue(label, value, 1, 1024);
}

std::string trimAsciiWhitespace(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char current) {
        return std::isspace(current) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char current) {
        return std::isspace(current) != 0;
    }).base();
    if (first >= last) {
        return "";
    }

    return std::string(first, last);
}

std::string toUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char current) {
        return static_cast<char>(std::toupper(current));
    });
    return value;
}

bool isReservedGameplayShortcut(sf::Keyboard::Key key) {
    return key == sf::Keyboard::Escape
        || key == sf::Keyboard::P
        || key == sf::Keyboard::K
        || key == sf::Keyboard::Space;
}

std::optional<sf::Keyboard::Key> parseShortcutKeyToken(const std::string& rawToken) {
    const std::string token = toUpperAscii(trimAsciiWhitespace(rawToken));
    if (token.empty()) {
        return std::nullopt;
    }

    if (token == "1" || token == "NUM1") return sf::Keyboard::Num1;
    if (token == "2" || token == "NUM2") return sf::Keyboard::Num2;
    if (token == "3" || token == "NUM3") return sf::Keyboard::Num3;
    if (token == "4" || token == "NUM4") return sf::Keyboard::Num4;
    if (token == "5" || token == "NUM5") return sf::Keyboard::Num5;
    if (token == "6" || token == "NUM6") return sf::Keyboard::Num6;
    if (token == "7" || token == "NUM7") return sf::Keyboard::Num7;
    if (token == "8" || token == "NUM8") return sf::Keyboard::Num8;
    if (token == "9" || token == "NUM9") return sf::Keyboard::Num9;
    if (token == "0" || token == "NUM0") return sf::Keyboard::Num0;
    if (token == "NUMPAD1") return sf::Keyboard::Numpad1;
    if (token == "NUMPAD2") return sf::Keyboard::Numpad2;
    if (token == "NUMPAD3") return sf::Keyboard::Numpad3;
    if (token == "NUMPAD4") return sf::Keyboard::Numpad4;
    if (token == "NUMPAD5") return sf::Keyboard::Numpad5;
    if (token == "NUMPAD6") return sf::Keyboard::Numpad6;
    if (token == "NUMPAD7") return sf::Keyboard::Numpad7;
    if (token == "NUMPAD8") return sf::Keyboard::Numpad8;
    if (token == "NUMPAD9") return sf::Keyboard::Numpad9;
    if (token == "NUMPAD0") return sf::Keyboard::Numpad0;
    if (token == "SPACE") return sf::Keyboard::Space;
    if (token == "ESCAPE") return sf::Keyboard::Escape;
    if (token == "TAB") return sf::Keyboard::Tab;
    if (token == "TILDE" || token == "GRAVE") return sf::Keyboard::Tilde;
    if (token == "F1") return sf::Keyboard::F1;
    if (token == "F2") return sf::Keyboard::F2;
    if (token == "F3") return sf::Keyboard::F3;
    if (token == "F4") return sf::Keyboard::F4;
    if (token == "F5") return sf::Keyboard::F5;
    if (token == "F6") return sf::Keyboard::F6;
    if (token == "F7") return sf::Keyboard::F7;
    if (token == "F8") return sf::Keyboard::F8;
    if (token == "F9") return sf::Keyboard::F9;
    if (token == "F10") return sf::Keyboard::F10;
    if (token == "F11") return sf::Keyboard::F11;
    if (token == "F12") return sf::Keyboard::F12;

    if (token.size() == 1 && token[0] >= 'A' && token[0] <= 'Z') {
        return static_cast<sf::Keyboard::Key>(sf::Keyboard::A + (token[0] - 'A'));
    }

    return std::nullopt;
}

void sanitizeCheatcodeShortcut(const char* label,
                               sf::Keyboard::Key defaultKey,
                               sf::Keyboard::Key& key) {
    if (key == sf::Keyboard::Unknown) {
        std::cerr << "GameConfig: Invalid cheatcode shortcut for " << label
                  << ", falling back to default.\n";
        key = defaultKey;
    }

    if (isReservedGameplayShortcut(key)) {
        std::cerr << "GameConfig: Cheatcode shortcut for " << label
                  << " conflicts with a reserved gameplay shortcut, falling back to default.\n";
        key = defaultKey;
    }
}

void sanitizeCheatcodeShortcutSet(sf::Keyboard::Key& weatherKey,
                                  sf::Keyboard::Key& chestKey,
                                  sf::Keyboard::Key& infernalKey) {
    if (weatherKey == chestKey) {
        std::cerr << "GameConfig: Cheatcode shortcuts weather_fog and chest_loot conflict, resetting chest_loot to default.\n";
        chestKey = sf::Keyboard::Num2;
    }

    if (weatherKey == infernalKey || chestKey == infernalKey) {
        std::cerr << "GameConfig: Cheatcode shortcut infernal_piece conflicts with another cheat shortcut, resetting infernal_piece to default.\n";
        infernalKey = sf::Keyboard::Num3;
    }

    if (weatherKey == chestKey) {
        chestKey = sf::Keyboard::F2;
    }
    if (weatherKey == infernalKey || chestKey == infernalKey) {
        infernalKey = sf::Keyboard::F3;
    }
}

}

GameConfig::GameConfig() { setDefaults(); }

void GameConfig::setDefaults() {
    m_mapRadius = 50;
    m_cellSizePx = 16;
    m_numMines = 2;
    m_numFarms = 3;
    m_minPublicBuildingDistance = 10;
    m_playerSpawnZonePercent = 25;
    m_aiSpawnZonePercent = 25;
    m_terrainNoiseScale = 14;
    m_terrainOctaves = 3;
    m_dirtCoveragePercent = 14;
    m_waterCoveragePercent = 4;
    m_numDirtBlobs = 6;
    m_dirtBlobMinRadius = 2;
    m_dirtBlobMaxRadius = 5;
    m_numLakes = 3;
    m_lakeMinRadius = 2;
    m_lakeMaxRadius = 3;

    m_startingGold = 0;
    m_mineIncomePerCellPerTurn = 10;
    m_farmIncomePerCellPerTurn = 5;
    m_barracksCost = 50;
    m_woodWallCost = 20;
    m_stoneWallCost = 40;
    m_arenaCost = 60;
    m_barracksRepairCostPerCell = 7;
    m_arenaRepairCostPerCell = 7;
    m_bridgeRepairCostPerCell = 5;
    m_movementPointsPerTurn = 5;
    m_buildPointsPerTurn = 4;
    m_pawnMovePointCost = 1;
    m_knightMovePointCost = 2;
    m_bishopMovePointCost = 2;
    m_rookMovePointCost = 4;
    m_queenMovePointCost = 4;
    m_kingMovePointCost = 2;
    m_barracksBuildPointCost = 3;
    m_woodWallBuildPointCost = 1;
    m_stoneWallBuildPointCost = 2;
    m_bridgeBuildPointCost = 2;
    m_arenaBuildPointCost = 4;
    m_pawnMoveAllowancePerTurn = 1;
    m_knightMoveAllowancePerTurn = 1;
    m_bishopMoveAllowancePerTurn = 1;
    m_rookMoveAllowancePerTurn = 1;
    m_queenMoveAllowancePerTurn = 1;
    m_kingMoveAllowancePerTurn = 1;
    m_pawnRecruitCost = 10;
    m_knightRecruitCost = 30;
    m_bishopRecruitCost = 30;
    m_rookRecruitCost = 60;
    m_pawnUpkeepCost = 1;
    m_knightUpkeepCost = 2;
    m_bishopUpkeepCost = 2;
    m_rookUpkeepCost = 4;
    m_queenUpkeepCost = 7;
    m_kingUpkeepCost = 0;
    m_upgradePawnToKnightCost = 20;
    m_upgradePawnToBishopCost = 20;
    m_upgradeToRookCost = 50;

    m_pawnTurns = 2;
    m_knightTurns = 4;
    m_bishopTurns = 4;
    m_rookTurns = 6;

    m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillPawn)] = makeXPRewardProfile(20, 18, 200, 1);
    m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillKnight)] = makeXPRewardProfile(50, 16, 200, 1);
    m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillBishop)] = makeXPRewardProfile(50, 16, 200, 1);
    m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillRook)] = makeXPRewardProfile(100, 12, 200, 1);
    m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillQueen)] = makeXPRewardProfile(300, 10, 200, 1);
    m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::DestroyBlock)] = makeXPRewardProfile(10, 15, 200, 1);
    m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::ArenaPerTurn)] = makeXPRewardProfile(10, 15, 200, 1);
    m_thresholdPawnToKnightOrBishop = 100;
    m_thresholdToRook = 300;

    m_woodWallHP = 1;
    m_stoneWallHP = 3;
    m_barracksCellHP = 1;
    m_woodWallDestroyedCellsRequired = 3;
    m_stoneWallDestroyedCellsRequired = 3;
    m_barracksDestroyedCellsRequired = 3;
    m_bridgeDestroyedCellsRequired = 3;
    m_arenaDestroyedCellsRequired = 3;
    m_globalMaxRange = 8;

    m_barracksWidth = 4; m_barracksHeight = 3;
    m_churchWidth = 4; m_churchHeight = 3;
    m_mineWidth = 6; m_mineHeight = 6;
    m_farmWidth = 6; m_farmHeight = 4;
    m_arenaWidth = 4; m_arenaHeight = 4;
    m_chestMinSpawnTurn = 4;
    m_chestRespawnCooldownTurns = 4;
    m_chestSpawnRetryTurns = 1;
    m_chestWeibullShapeTimes100 = 180;
    m_chestWeibullScaleTurns = 6;
    m_chestMinDistanceFromKings = 6;
    m_chestGoldRewardAmount = 35;
    m_chestMovementBonusAmount = 1;
    m_chestBuildBonusAmount = 1;
    m_chestLateGameTurn = 10;
    m_chestEarlyGoldWeight = 8;
    m_chestEarlyMovementBonusWeight = 3;
    m_chestEarlyBuildBonusWeight = 3;
    m_chestLateGoldWeight = 4;
    m_chestLateMovementBonusWeight = 6;
    m_chestLateBuildBonusWeight = 6;
    m_infernalMinSpawnTurn = 3;
    m_infernalRespawnCooldownTurns = 4;
    m_infernalSpawnRetryTurns = 1;
    m_infernalPoissonLambdaBaseTimes1000 = 20;
    m_infernalPoissonLambdaPerDebtTimes1000 = 12;
    m_infernalPoissonLambdaCapTimes1000 = 250;
    m_infernalBloodDebtDecayPercent = 95;
    m_infernalDebtPawn = 1;
    m_infernalDebtKnight = 2;
    m_infernalDebtBishop = 2;
    m_infernalDebtRook = 3;
    m_infernalDebtQueen = 5;
    m_infernalDebtStructureDamage = 1;
    m_infernalTargetWeightPawn = 8;
    m_infernalTargetWeightKnight = 14;
    m_infernalTargetWeightBishop = 14;
    m_infernalTargetWeightRook = 26;
    m_infernalTargetWeightQueen = 38;
    m_infernalSearchingRandomMoveChanceTimes1000 = 333;

    m_weatherCooldownMinTurns = 5;
    m_weatherArrivalGammaShapeTimes100 = 240;
    m_weatherArrivalGammaScaleTimes100 = 220;
    m_weatherDurationGammaShapeTimes100 = 260;
    m_weatherDurationGammaScaleTimes100 = 180;
    m_weatherSpeedBlocksPer100Turns = 50;
    m_weatherDirectionWeights.fill(1);
    m_weatherEntryCenterWeightTimes100 = 180;
    m_weatherEntryCornerWeightTimes100 = 70;
    m_weatherCoverageMinPercent = 25;
    m_weatherCoverageMaxPercent = 35;
    m_weatherAspectRatioMinTimes100 = 180;
    m_weatherAspectRatioMaxTimes100 = 260;
    m_weatherShapeNoiseCellSpan = 6;
    m_weatherShapeNoiseAmplitudePercent = 18;
    m_weatherEdgeSoftnessPercent = 18;
    m_weatherAlphaBasePercent = 48;
    m_weatherAlphaMinPercent = 22;
    m_weatherAlphaMaxPercent = 82;
    m_weatherDensityMuTimes100 = -12;
    m_weatherDensitySigmaTimes100 = 35;

    m_cheatcodeEnabled = false;
    m_cheatcodeWeatherShortcut = sf::Keyboard::Num1;
    m_cheatcodeChestShortcut = sf::Keyboard::Num2;
    m_cheatcodeInfernalShortcut = sf::Keyboard::Num3;

    alignChunkedStructureDimensions("barracks", BuildingType::Barracks, m_barracksWidth, m_barracksHeight);
    alignChunkedStructureDimensions("church", BuildingType::Church, m_churchWidth, m_churchHeight);
    alignChunkedStructureDimensions("mine", BuildingType::Mine, m_mineWidth, m_mineHeight);
    alignChunkedStructureDimensions("farm", BuildingType::Farm, m_farmWidth, m_farmHeight);
    alignChunkedStructureDimensions("arena", BuildingType::Arena, m_arenaWidth, m_arenaHeight);
}

std::string GameConfig::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string GameConfig::extractSection(const std::string& json, const std::string& key) {
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

int GameConfig::extractInt(const std::string& json, const std::string& key, int defaultVal) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    bool negative = false;
    if (pos < json.size() && json[pos] == '-') { negative = true; ++pos; }
    int val = 0;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (json[pos] - '0');
        ++pos;
    }
    return negative ? -val : val;
}

bool GameConfig::extractBool(const std::string& json, const std::string& key, bool defaultVal) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])) != 0) ++pos;

    if (json.compare(pos, 4, "true") == 0) return true;
    if (json.compare(pos, 5, "false") == 0) return false;
    if (pos < json.size() && json[pos] == '1') return true;
    if (pos < json.size() && json[pos] == '0') return false;
    return defaultVal;
}

std::string GameConfig::extractString(const std::string& json, const std::string& key, const std::string& defaultVal) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])) != 0) ++pos;
    if (pos >= json.size() || json[pos] != '"') return defaultVal;

    ++pos;
    const std::size_t start = pos;
    while (pos < json.size()) {
        if (json[pos] == '"' && (pos == start || json[pos - 1] != '\\')) {
            return json.substr(start, pos - start);
        }
        ++pos;
    }

    return defaultVal;
}

bool GameConfig::loadFromFile(const std::string& filepath) {
    std::string json = readFile(filepath);
    if (json.empty()) {
        std::cerr << "GameConfig: Could not load " << filepath << ", using defaults.\n";
        return false;
    }

    const std::string wrappedGameSection = extractSection(json, "game");
    const std::string root = wrappedGameSection.empty() ? json : wrappedGameSection;

    std::string mapSec = extractSection(root, "map");
    if (!mapSec.empty()) {
        m_mapRadius = extractInt(mapSec, "radius", m_mapRadius);
        m_cellSizePx = extractInt(mapSec, "cell_size_px", m_cellSizePx);
        m_numMines = extractInt(mapSec, "num_mines", m_numMines);
        m_numFarms = extractInt(mapSec, "num_farms", m_numFarms);
        m_minPublicBuildingDistance = extractInt(mapSec, "min_public_building_distance", m_minPublicBuildingDistance);
        m_playerSpawnZonePercent = extractInt(mapSec, "player_spawn_zone_percent", m_playerSpawnZonePercent);
        m_aiSpawnZonePercent = extractInt(mapSec, "ai_spawn_zone_percent", m_aiSpawnZonePercent);
        m_terrainNoiseScale = extractInt(mapSec, "terrain_noise_scale", m_terrainNoiseScale);
        m_terrainOctaves = extractInt(mapSec, "terrain_octaves", m_terrainOctaves);
        m_dirtCoveragePercent = extractInt(mapSec, "dirt_coverage_percent", m_dirtCoveragePercent);
        m_waterCoveragePercent = extractInt(mapSec, "water_coverage_percent", m_waterCoveragePercent);
        m_numDirtBlobs = extractInt(mapSec, "num_dirt_blobs", m_numDirtBlobs);
        m_dirtBlobMinRadius = extractInt(mapSec, "dirt_blob_min_radius", m_dirtBlobMinRadius);
        m_dirtBlobMaxRadius = extractInt(mapSec, "dirt_blob_max_radius", m_dirtBlobMaxRadius);
        m_numLakes = extractInt(mapSec, "num_lakes", m_numLakes);
        m_lakeMinRadius = extractInt(mapSec, "lake_min_radius", m_lakeMinRadius);
        m_lakeMaxRadius = extractInt(mapSec, "lake_max_radius", m_lakeMaxRadius);
    }

    std::string econSec = extractSection(root, "economy");
    if (!econSec.empty()) {
        m_startingGold = extractInt(econSec, "starting_gold", m_startingGold);
        m_mineIncomePerCellPerTurn = extractInt(econSec, "mine_income_per_cell_per_turn", m_mineIncomePerCellPerTurn);
        m_farmIncomePerCellPerTurn = extractInt(econSec, "farm_income_per_cell_per_turn", m_farmIncomePerCellPerTurn);
        m_barracksCost = extractInt(econSec, "barracks_cost", m_barracksCost);
        m_woodWallCost = extractInt(econSec, "wood_wall_cost", m_woodWallCost);
        m_stoneWallCost = extractInt(econSec, "stone_wall_cost", m_stoneWallCost);
        m_arenaCost = extractInt(econSec, "arena_cost", m_arenaCost);
        m_barracksRepairCostPerCell = extractInt(
            econSec, "barracks_repair_cost_per_cell", m_barracksRepairCostPerCell);
        m_arenaRepairCostPerCell = extractInt(
            econSec, "arena_repair_cost_per_cell", m_arenaRepairCostPerCell);
        m_bridgeRepairCostPerCell = extractInt(
            econSec, "bridge_repair_cost_per_cell", m_bridgeRepairCostPerCell);
        m_pawnRecruitCost = extractInt(econSec, "pawn_recruit_cost", m_pawnRecruitCost);
        m_knightRecruitCost = extractInt(econSec, "knight_recruit_cost", m_knightRecruitCost);
        m_bishopRecruitCost = extractInt(econSec, "bishop_recruit_cost", m_bishopRecruitCost);
        m_rookRecruitCost = extractInt(econSec, "rook_recruit_cost", m_rookRecruitCost);
        m_pawnUpkeepCost = extractInt(econSec, "pawn_upkeep_cost", m_pawnUpkeepCost);
        m_knightUpkeepCost = extractInt(econSec, "knight_upkeep_cost", m_knightUpkeepCost);
        m_bishopUpkeepCost = extractInt(econSec, "bishop_upkeep_cost", m_bishopUpkeepCost);
        m_rookUpkeepCost = extractInt(econSec, "rook_upkeep_cost", m_rookUpkeepCost);
        m_queenUpkeepCost = extractInt(econSec, "queen_upkeep_cost", m_queenUpkeepCost);
        m_kingUpkeepCost = extractInt(econSec, "king_upkeep_cost", m_kingUpkeepCost);
        m_upgradePawnToKnightCost = extractInt(econSec, "upgrade_pawn_to_knight_cost", m_upgradePawnToKnightCost);
        m_upgradePawnToBishopCost = extractInt(econSec, "upgrade_pawn_to_bishop_cost", m_upgradePawnToBishopCost);
        m_upgradeToRookCost = extractInt(econSec, "upgrade_to_rook_cost", m_upgradeToRookCost);
    }

    m_startingGold = clampNonNegativeConfigValue("economy.starting_gold", m_startingGold);
    m_mineIncomePerCellPerTurn = clampNonNegativeConfigValue(
        "economy.mine_income_per_cell_per_turn", m_mineIncomePerCellPerTurn);
    m_farmIncomePerCellPerTurn = clampNonNegativeConfigValue(
        "economy.farm_income_per_cell_per_turn", m_farmIncomePerCellPerTurn);
    m_barracksCost = clampNonNegativeConfigValue("economy.barracks_cost", m_barracksCost);
    m_woodWallCost = clampNonNegativeConfigValue("economy.wood_wall_cost", m_woodWallCost);
    m_stoneWallCost = clampNonNegativeConfigValue("economy.stone_wall_cost", m_stoneWallCost);
    m_arenaCost = clampNonNegativeConfigValue("economy.arena_cost", m_arenaCost);
    m_barracksRepairCostPerCell = clampNonNegativeConfigValue(
        "economy.barracks_repair_cost_per_cell", m_barracksRepairCostPerCell);
    m_arenaRepairCostPerCell = clampNonNegativeConfigValue(
        "economy.arena_repair_cost_per_cell", m_arenaRepairCostPerCell);
    m_bridgeRepairCostPerCell = clampNonNegativeConfigValue(
        "economy.bridge_repair_cost_per_cell", m_bridgeRepairCostPerCell);
    m_pawnRecruitCost = clampNonNegativeConfigValue("economy.pawn_recruit_cost", m_pawnRecruitCost);
    m_knightRecruitCost = clampNonNegativeConfigValue("economy.knight_recruit_cost", m_knightRecruitCost);
    m_bishopRecruitCost = clampNonNegativeConfigValue("economy.bishop_recruit_cost", m_bishopRecruitCost);
    m_rookRecruitCost = clampNonNegativeConfigValue("economy.rook_recruit_cost", m_rookRecruitCost);
    m_pawnUpkeepCost = clampNonNegativeConfigValue("economy.pawn_upkeep_cost", m_pawnUpkeepCost);
    m_knightUpkeepCost = clampNonNegativeConfigValue("economy.knight_upkeep_cost", m_knightUpkeepCost);
    m_bishopUpkeepCost = clampNonNegativeConfigValue("economy.bishop_upkeep_cost", m_bishopUpkeepCost);
    m_rookUpkeepCost = clampNonNegativeConfigValue("economy.rook_upkeep_cost", m_rookUpkeepCost);
    m_queenUpkeepCost = clampNonNegativeConfigValue("economy.queen_upkeep_cost", m_queenUpkeepCost);
    m_kingUpkeepCost = clampNonNegativeConfigValue("economy.king_upkeep_cost", m_kingUpkeepCost);
    m_upgradePawnToKnightCost = clampNonNegativeConfigValue(
        "economy.upgrade_pawn_to_knight_cost", m_upgradePawnToKnightCost);
    m_upgradePawnToBishopCost = clampNonNegativeConfigValue(
        "economy.upgrade_pawn_to_bishop_cost", m_upgradePawnToBishopCost);
    m_upgradeToRookCost = clampNonNegativeConfigValue("economy.upgrade_to_rook_cost", m_upgradeToRookCost);

    std::string turnPointSec = extractSection(root, "turn_points");
    if (!turnPointSec.empty()) {
        m_movementPointsPerTurn = extractInt(turnPointSec, "movement_points_per_turn", m_movementPointsPerTurn);
        m_buildPointsPerTurn = extractInt(turnPointSec, "build_points_per_turn", m_buildPointsPerTurn);
        m_pawnMovePointCost = extractInt(turnPointSec, "pawn_move_point_cost", m_pawnMovePointCost);
        m_knightMovePointCost = extractInt(turnPointSec, "knight_move_point_cost", m_knightMovePointCost);
        m_bishopMovePointCost = extractInt(turnPointSec, "bishop_move_point_cost", m_bishopMovePointCost);
        m_rookMovePointCost = extractInt(turnPointSec, "rook_move_point_cost", m_rookMovePointCost);
        m_queenMovePointCost = extractInt(turnPointSec, "queen_move_point_cost", m_queenMovePointCost);
        m_kingMovePointCost = extractInt(turnPointSec, "king_move_point_cost", m_kingMovePointCost);
        m_barracksBuildPointCost = extractInt(turnPointSec, "barracks_build_point_cost", m_barracksBuildPointCost);
        m_woodWallBuildPointCost = extractInt(turnPointSec, "wood_wall_build_point_cost", m_woodWallBuildPointCost);
        m_stoneWallBuildPointCost = extractInt(turnPointSec, "stone_wall_build_point_cost", m_stoneWallBuildPointCost);
        m_bridgeBuildPointCost = extractInt(turnPointSec, "bridge_build_point_cost", m_bridgeBuildPointCost);
        m_arenaBuildPointCost = extractInt(turnPointSec, "arena_build_point_cost", m_arenaBuildPointCost);
        m_pawnMoveAllowancePerTurn = extractInt(turnPointSec, "pawn_move_allowance_per_turn", m_pawnMoveAllowancePerTurn);
        m_knightMoveAllowancePerTurn = extractInt(turnPointSec, "knight_move_allowance_per_turn", m_knightMoveAllowancePerTurn);
        m_bishopMoveAllowancePerTurn = extractInt(turnPointSec, "bishop_move_allowance_per_turn", m_bishopMoveAllowancePerTurn);
        m_rookMoveAllowancePerTurn = extractInt(turnPointSec, "rook_move_allowance_per_turn", m_rookMoveAllowancePerTurn);
        m_queenMoveAllowancePerTurn = extractInt(turnPointSec, "queen_move_allowance_per_turn", m_queenMoveAllowancePerTurn);
        m_kingMoveAllowancePerTurn = extractInt(turnPointSec, "king_move_allowance_per_turn", m_kingMoveAllowancePerTurn);
    }

    m_movementPointsPerTurn = clampNonNegativeConfigValue(
        "turn_points.movement_points_per_turn", m_movementPointsPerTurn);
    m_buildPointsPerTurn = clampNonNegativeConfigValue(
        "turn_points.build_points_per_turn", m_buildPointsPerTurn);
    m_pawnMovePointCost = clampNonNegativeConfigValue(
        "turn_points.pawn_move_point_cost", m_pawnMovePointCost);
    m_knightMovePointCost = clampNonNegativeConfigValue(
        "turn_points.knight_move_point_cost", m_knightMovePointCost);
    m_bishopMovePointCost = clampNonNegativeConfigValue(
        "turn_points.bishop_move_point_cost", m_bishopMovePointCost);
    m_rookMovePointCost = clampNonNegativeConfigValue(
        "turn_points.rook_move_point_cost", m_rookMovePointCost);
    m_queenMovePointCost = clampNonNegativeConfigValue(
        "turn_points.queen_move_point_cost", m_queenMovePointCost);
    m_kingMovePointCost = clampNonNegativeConfigValue(
        "turn_points.king_move_point_cost", m_kingMovePointCost);
    m_barracksBuildPointCost = clampNonNegativeConfigValue(
        "turn_points.barracks_build_point_cost", m_barracksBuildPointCost);
    m_woodWallBuildPointCost = clampNonNegativeConfigValue(
        "turn_points.wood_wall_build_point_cost", m_woodWallBuildPointCost);
    m_stoneWallBuildPointCost = clampNonNegativeConfigValue(
        "turn_points.stone_wall_build_point_cost", m_stoneWallBuildPointCost);
    m_bridgeBuildPointCost = clampNonNegativeConfigValue(
        "turn_points.bridge_build_point_cost", m_bridgeBuildPointCost);
    m_arenaBuildPointCost = clampNonNegativeConfigValue(
        "turn_points.arena_build_point_cost", m_arenaBuildPointCost);
    m_pawnMoveAllowancePerTurn = clampNonNegativeConfigValue(
        "turn_points.pawn_move_allowance_per_turn", m_pawnMoveAllowancePerTurn);
    m_knightMoveAllowancePerTurn = clampNonNegativeConfigValue(
        "turn_points.knight_move_allowance_per_turn", m_knightMoveAllowancePerTurn);
    m_bishopMoveAllowancePerTurn = clampNonNegativeConfigValue(
        "turn_points.bishop_move_allowance_per_turn", m_bishopMoveAllowancePerTurn);
    m_rookMoveAllowancePerTurn = clampNonNegativeConfigValue(
        "turn_points.rook_move_allowance_per_turn", m_rookMoveAllowancePerTurn);
    m_queenMoveAllowancePerTurn = clampNonNegativeConfigValue(
        "turn_points.queen_move_allowance_per_turn", m_queenMoveAllowancePerTurn);
    m_kingMoveAllowancePerTurn = clampNonNegativeConfigValue(
        "turn_points.king_move_allowance_per_turn", m_kingMoveAllowancePerTurn);

    std::string prodSec = extractSection(root, "production");
    if (!prodSec.empty()) {
        m_pawnTurns = extractInt(prodSec, "pawn_turns", m_pawnTurns);
        m_knightTurns = extractInt(prodSec, "knight_turns", m_knightTurns);
        m_bishopTurns = extractInt(prodSec, "bishop_turns", m_bishopTurns);
        m_rookTurns = extractInt(prodSec, "rook_turns", m_rookTurns);
    }

    std::string xpSec = extractSection(root, "xp");
    if (!xpSec.empty()) {
        m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillPawn)].mean = extractInt(
            xpSec, "kill_pawn", m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillPawn)].mean);
        m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillKnight)].mean = extractInt(
            xpSec, "kill_knight", m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillKnight)].mean);
        m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillBishop)].mean = extractInt(
            xpSec, "kill_bishop", m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillBishop)].mean);
        m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillRook)].mean = extractInt(
            xpSec, "kill_rook", m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillRook)].mean);
        m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillQueen)].mean = extractInt(
            xpSec, "kill_queen", m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::KillQueen)].mean);
        m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::DestroyBlock)].mean = extractInt(
            xpSec, "destroy_block", m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::DestroyBlock)].mean);
        m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::ArenaPerTurn)].mean = extractInt(
            xpSec, "arena_per_turn", m_xpRewardProfiles[xpRewardSourceIndex(XPRewardSource::ArenaPerTurn)].mean);
        m_thresholdPawnToKnightOrBishop = extractInt(xpSec, "threshold_pawn_to_knight_or_bishop", m_thresholdPawnToKnightOrBishop);
        m_thresholdToRook = extractInt(xpSec, "threshold_to_rook", m_thresholdToRook);

        const std::string xpThresholdSec = extractSection(xpSec, "thresholds");
        if (!xpThresholdSec.empty()) {
            m_thresholdPawnToKnightOrBishop = extractInt(
                xpThresholdSec,
                "pawn_to_knight_or_bishop",
                m_thresholdPawnToKnightOrBishop);
            m_thresholdToRook = extractInt(
                xpThresholdSec,
                "to_rook",
                m_thresholdToRook);
        }

        const std::string xpSourcesSec = extractSection(xpSec, "sources");
        if (!xpSourcesSec.empty()) {
            const auto parseXPProfile = [&](XPRewardSource source, const char* key) {
                const std::string profileSec = extractSection(xpSourcesSec, key);
                if (profileSec.empty()) {
                    return;
                }

                XPRewardProfile& profile = m_xpRewardProfiles[xpRewardSourceIndex(source)];
                profile.mean = extractInt(profileSec, "mean", profile.mean);
                profile.sigmaMultiplierTimes100 = extractInt(
                    profileSec,
                    "sigma_multiplier_times_100",
                    profile.sigmaMultiplierTimes100);
                profile.clampSigmaMultiplierTimes100 = extractInt(
                    profileSec,
                    "clamp_sigma_multiplier_times_100",
                    profile.clampSigmaMultiplierTimes100);
                profile.minimum = extractInt(profileSec, "minimum", profile.minimum);
            };

            parseXPProfile(XPRewardSource::KillPawn, "kill_pawn");
            parseXPProfile(XPRewardSource::KillKnight, "kill_knight");
            parseXPProfile(XPRewardSource::KillBishop, "kill_bishop");
            parseXPProfile(XPRewardSource::KillRook, "kill_rook");
            parseXPProfile(XPRewardSource::KillQueen, "kill_queen");
            parseXPProfile(XPRewardSource::DestroyBlock, "destroy_block");
            parseXPProfile(XPRewardSource::ArenaPerTurn, "arena_per_turn");
        }
    }

    const auto clampXPProfile = [&](XPRewardSource source, const char* label) {
        XPRewardProfile& profile = m_xpRewardProfiles[xpRewardSourceIndex(source)];
        const std::string baseLabel = std::string("xp.sources.") + label;
        profile.mean = clampNonNegativeConfigValue((baseLabel + ".mean").c_str(), profile.mean);
        profile.sigmaMultiplierTimes100 = clampRangedConfigValue(
            baseLabel + ".sigma_multiplier_times_100",
            profile.sigmaMultiplierTimes100,
            0,
            1000);
        profile.clampSigmaMultiplierTimes100 = clampRangedConfigValue(
            baseLabel + ".clamp_sigma_multiplier_times_100",
            profile.clampSigmaMultiplierTimes100,
            0,
            500);
        profile.minimum = clampNonNegativeConfigValue((baseLabel + ".minimum").c_str(), profile.minimum);
    };

    clampXPProfile(XPRewardSource::KillPawn, "kill_pawn");
    clampXPProfile(XPRewardSource::KillKnight, "kill_knight");
    clampXPProfile(XPRewardSource::KillBishop, "kill_bishop");
    clampXPProfile(XPRewardSource::KillRook, "kill_rook");
    clampXPProfile(XPRewardSource::KillQueen, "kill_queen");
    clampXPProfile(XPRewardSource::DestroyBlock, "destroy_block");
    clampXPProfile(XPRewardSource::ArenaPerTurn, "arena_per_turn");

    std::string combatSec = extractSection(root, "combat");
    if (!combatSec.empty()) {
        m_woodWallHP = extractInt(combatSec, "wood_wall_hp", m_woodWallHP);
        m_stoneWallHP = extractInt(combatSec, "stone_wall_hp", m_stoneWallHP);
        m_barracksCellHP = extractInt(combatSec, "barracks_cell_hp", m_barracksCellHP);
        m_woodWallDestroyedCellsRequired = extractInt(
            combatSec,
            "wood_wall_destroyed_cells_required",
            m_woodWallDestroyedCellsRequired);
        m_stoneWallDestroyedCellsRequired = extractInt(
            combatSec,
            "stone_wall_destroyed_cells_required",
            m_stoneWallDestroyedCellsRequired);
        m_barracksDestroyedCellsRequired = extractInt(
            combatSec,
            "barracks_destroyed_cells_required",
            m_barracksDestroyedCellsRequired);
        m_bridgeDestroyedCellsRequired = extractInt(
            combatSec,
            "bridge_destroyed_cells_required",
            m_bridgeDestroyedCellsRequired);
        m_arenaDestroyedCellsRequired = extractInt(
            combatSec,
            "arena_destroyed_cells_required",
            m_arenaDestroyedCellsRequired);
        m_globalMaxRange = extractInt(combatSec, "global_max_range", m_globalMaxRange);
    }

    m_woodWallDestroyedCellsRequired = clampPositiveConfigValue(
        "combat.wood_wall_destroyed_cells_required",
        m_woodWallDestroyedCellsRequired);
    m_stoneWallDestroyedCellsRequired = clampPositiveConfigValue(
        "combat.stone_wall_destroyed_cells_required",
        m_stoneWallDestroyedCellsRequired);
    m_barracksDestroyedCellsRequired = clampPositiveConfigValue(
        "combat.barracks_destroyed_cells_required",
        m_barracksDestroyedCellsRequired);
    m_bridgeDestroyedCellsRequired = clampPositiveConfigValue(
        "combat.bridge_destroyed_cells_required",
        m_bridgeDestroyedCellsRequired);
    m_arenaDestroyedCellsRequired = clampPositiveConfigValue(
        "combat.arena_destroyed_cells_required",
        m_arenaDestroyedCellsRequired);

    std::string buildSec = extractSection(root, "buildings");
    if (!buildSec.empty()) {
        m_barracksWidth = extractInt(buildSec, "barracks_width", m_barracksWidth);
        m_barracksHeight = extractInt(buildSec, "barracks_height", m_barracksHeight);
        m_churchWidth = extractInt(buildSec, "church_width", m_churchWidth);
        m_churchHeight = extractInt(buildSec, "church_height", m_churchHeight);
        m_mineWidth = extractInt(buildSec, "mine_width", m_mineWidth);
        m_mineHeight = extractInt(buildSec, "mine_height", m_mineHeight);
        m_farmWidth = extractInt(buildSec, "farm_width", m_farmWidth);
        m_farmHeight = extractInt(buildSec, "farm_height", m_farmHeight);
        m_arenaWidth = extractInt(buildSec, "arena_width", m_arenaWidth);
        m_arenaHeight = extractInt(buildSec, "arena_height", m_arenaHeight);
    }

    std::string chestSec = extractSection(root, "chests");
    if (!chestSec.empty()) {
        m_chestMinSpawnTurn = extractInt(chestSec, "min_spawn_turn", m_chestMinSpawnTurn);
        m_chestRespawnCooldownTurns = extractInt(
            chestSec, "respawn_cooldown_turns", m_chestRespawnCooldownTurns);
        m_chestSpawnRetryTurns = extractInt(chestSec, "spawn_retry_turns", m_chestSpawnRetryTurns);
        m_chestWeibullShapeTimes100 = extractInt(
            chestSec, "weibull_shape_times_100", m_chestWeibullShapeTimes100);
        m_chestWeibullScaleTurns = extractInt(chestSec, "weibull_scale_turns", m_chestWeibullScaleTurns);
        m_chestMinDistanceFromKings = extractInt(
            chestSec, "min_distance_from_kings", m_chestMinDistanceFromKings);
        m_chestGoldRewardAmount = extractInt(chestSec, "gold_reward_amount", m_chestGoldRewardAmount);
        m_chestMovementBonusAmount = extractInt(
            chestSec, "movement_bonus_amount", m_chestMovementBonusAmount);
        m_chestBuildBonusAmount = extractInt(chestSec, "build_bonus_amount", m_chestBuildBonusAmount);
        m_chestLateGameTurn = extractInt(chestSec, "late_game_turn", m_chestLateGameTurn);
        m_chestEarlyGoldWeight = extractInt(chestSec, "early_gold_weight", m_chestEarlyGoldWeight);
        m_chestEarlyMovementBonusWeight = extractInt(
            chestSec, "early_movement_bonus_weight", m_chestEarlyMovementBonusWeight);
        m_chestEarlyBuildBonusWeight = extractInt(
            chestSec, "early_build_bonus_weight", m_chestEarlyBuildBonusWeight);
        m_chestLateGoldWeight = extractInt(chestSec, "late_gold_weight", m_chestLateGoldWeight);
        m_chestLateMovementBonusWeight = extractInt(
            chestSec, "late_movement_bonus_weight", m_chestLateMovementBonusWeight);
        m_chestLateBuildBonusWeight = extractInt(
            chestSec, "late_build_bonus_weight", m_chestLateBuildBonusWeight);
    }

    m_chestMinSpawnTurn = clampNonNegativeConfigValue("chests.min_spawn_turn", m_chestMinSpawnTurn);
    m_chestRespawnCooldownTurns = clampNonNegativeConfigValue(
        "chests.respawn_cooldown_turns", m_chestRespawnCooldownTurns);
    m_chestSpawnRetryTurns = clampNonNegativeConfigValue(
        "chests.spawn_retry_turns", m_chestSpawnRetryTurns);
    m_chestWeibullShapeTimes100 = clampNonNegativeConfigValue(
        "chests.weibull_shape_times_100", m_chestWeibullShapeTimes100);
    m_chestWeibullScaleTurns = clampNonNegativeConfigValue(
        "chests.weibull_scale_turns", m_chestWeibullScaleTurns);
    m_chestMinDistanceFromKings = clampNonNegativeConfigValue(
        "chests.min_distance_from_kings", m_chestMinDistanceFromKings);
    m_chestGoldRewardAmount = clampNonNegativeConfigValue(
        "chests.gold_reward_amount", m_chestGoldRewardAmount);
    m_chestMovementBonusAmount = clampNonNegativeConfigValue(
        "chests.movement_bonus_amount", m_chestMovementBonusAmount);
    m_chestBuildBonusAmount = clampNonNegativeConfigValue(
        "chests.build_bonus_amount", m_chestBuildBonusAmount);
    m_chestLateGameTurn = clampNonNegativeConfigValue("chests.late_game_turn", m_chestLateGameTurn);
    m_chestEarlyGoldWeight = clampNonNegativeConfigValue(
        "chests.early_gold_weight", m_chestEarlyGoldWeight);
    m_chestEarlyMovementBonusWeight = clampNonNegativeConfigValue(
        "chests.early_movement_bonus_weight", m_chestEarlyMovementBonusWeight);
    m_chestEarlyBuildBonusWeight = clampNonNegativeConfigValue(
        "chests.early_build_bonus_weight", m_chestEarlyBuildBonusWeight);
    m_chestLateGoldWeight = clampNonNegativeConfigValue(
        "chests.late_gold_weight", m_chestLateGoldWeight);
    m_chestLateMovementBonusWeight = clampNonNegativeConfigValue(
        "chests.late_movement_bonus_weight", m_chestLateMovementBonusWeight);
    m_chestLateBuildBonusWeight = clampNonNegativeConfigValue(
        "chests.late_build_bonus_weight", m_chestLateBuildBonusWeight);

    std::string infernalSec = extractSection(root, "infernal");
    if (!infernalSec.empty()) {
        m_infernalMinSpawnTurn = extractInt(infernalSec, "min_spawn_turn", m_infernalMinSpawnTurn);
        m_infernalRespawnCooldownTurns = extractInt(
            infernalSec, "respawn_cooldown_turns", m_infernalRespawnCooldownTurns);
        m_infernalSpawnRetryTurns = extractInt(
            infernalSec, "spawn_retry_turns", m_infernalSpawnRetryTurns);
        m_infernalPoissonLambdaBaseTimes1000 = extractInt(
            infernalSec, "poisson_lambda_base_times_1000", m_infernalPoissonLambdaBaseTimes1000);
        m_infernalPoissonLambdaPerDebtTimes1000 = extractInt(
            infernalSec, "poisson_lambda_per_debt_times_1000", m_infernalPoissonLambdaPerDebtTimes1000);
        m_infernalPoissonLambdaCapTimes1000 = extractInt(
            infernalSec, "poisson_lambda_cap_times_1000", m_infernalPoissonLambdaCapTimes1000);
        m_infernalBloodDebtDecayPercent = extractInt(
            infernalSec, "blood_debt_decay_percent", m_infernalBloodDebtDecayPercent);
        m_infernalDebtPawn = extractInt(infernalSec, "blood_debt_pawn", m_infernalDebtPawn);
        m_infernalDebtKnight = extractInt(infernalSec, "blood_debt_knight", m_infernalDebtKnight);
        m_infernalDebtBishop = extractInt(infernalSec, "blood_debt_bishop", m_infernalDebtBishop);
        m_infernalDebtRook = extractInt(infernalSec, "blood_debt_rook", m_infernalDebtRook);
        m_infernalDebtQueen = extractInt(infernalSec, "blood_debt_queen", m_infernalDebtQueen);
        m_infernalDebtStructureDamage = extractInt(
            infernalSec, "blood_debt_structure_damage", m_infernalDebtStructureDamage);
        m_infernalTargetWeightPawn = extractInt(
            infernalSec, "target_weight_pawn", m_infernalTargetWeightPawn);
        m_infernalTargetWeightKnight = extractInt(
            infernalSec, "target_weight_knight", m_infernalTargetWeightKnight);
        m_infernalTargetWeightBishop = extractInt(
            infernalSec, "target_weight_bishop", m_infernalTargetWeightBishop);
        m_infernalTargetWeightRook = extractInt(
            infernalSec, "target_weight_rook", m_infernalTargetWeightRook);
        m_infernalTargetWeightQueen = extractInt(
            infernalSec, "target_weight_queen", m_infernalTargetWeightQueen);
        m_infernalSearchingRandomMoveChanceTimes1000 = extractInt(
            infernalSec,
            "searching_random_move_chance_times_1000",
            m_infernalSearchingRandomMoveChanceTimes1000);
    }

    m_infernalMinSpawnTurn = clampNonNegativeConfigValue(
        "infernal.min_spawn_turn", m_infernalMinSpawnTurn);
    m_infernalRespawnCooldownTurns = clampNonNegativeConfigValue(
        "infernal.respawn_cooldown_turns", m_infernalRespawnCooldownTurns);
    m_infernalSpawnRetryTurns = clampNonNegativeConfigValue(
        "infernal.spawn_retry_turns", m_infernalSpawnRetryTurns);
    m_infernalPoissonLambdaBaseTimes1000 = clampNonNegativeConfigValue(
        "infernal.poisson_lambda_base_times_1000", m_infernalPoissonLambdaBaseTimes1000);
    m_infernalPoissonLambdaPerDebtTimes1000 = clampNonNegativeConfigValue(
        "infernal.poisson_lambda_per_debt_times_1000", m_infernalPoissonLambdaPerDebtTimes1000);
    m_infernalPoissonLambdaCapTimes1000 = clampNonNegativeConfigValue(
        "infernal.poisson_lambda_cap_times_1000", m_infernalPoissonLambdaCapTimes1000);
    m_infernalBloodDebtDecayPercent = std::clamp(
        m_infernalBloodDebtDecayPercent,
        0,
        100);
    m_infernalDebtPawn = clampNonNegativeConfigValue("infernal.blood_debt_pawn", m_infernalDebtPawn);
    m_infernalDebtKnight = clampNonNegativeConfigValue("infernal.blood_debt_knight", m_infernalDebtKnight);
    m_infernalDebtBishop = clampNonNegativeConfigValue("infernal.blood_debt_bishop", m_infernalDebtBishop);
    m_infernalDebtRook = clampNonNegativeConfigValue("infernal.blood_debt_rook", m_infernalDebtRook);
    m_infernalDebtQueen = clampNonNegativeConfigValue("infernal.blood_debt_queen", m_infernalDebtQueen);
    m_infernalDebtStructureDamage = clampNonNegativeConfigValue(
        "infernal.blood_debt_structure_damage", m_infernalDebtStructureDamage);
    m_infernalTargetWeightPawn = clampNonNegativeConfigValue(
        "infernal.target_weight_pawn", m_infernalTargetWeightPawn);
    m_infernalTargetWeightKnight = clampNonNegativeConfigValue(
        "infernal.target_weight_knight", m_infernalTargetWeightKnight);
    m_infernalTargetWeightBishop = clampNonNegativeConfigValue(
        "infernal.target_weight_bishop", m_infernalTargetWeightBishop);
    m_infernalTargetWeightRook = clampNonNegativeConfigValue(
        "infernal.target_weight_rook", m_infernalTargetWeightRook);
    m_infernalTargetWeightQueen = clampNonNegativeConfigValue(
        "infernal.target_weight_queen", m_infernalTargetWeightQueen);
    m_infernalSearchingRandomMoveChanceTimes1000 = clampRangedConfigValue(
        "infernal.searching_random_move_chance_times_1000",
        m_infernalSearchingRandomMoveChanceTimes1000,
        0,
        1000);

    std::string weatherSec = extractSection(root, "weather");
    if (!weatherSec.empty()) {
        m_weatherCooldownMinTurns = extractInt(
            weatherSec, "cooldown_min_turns", m_weatherCooldownMinTurns);
        m_weatherArrivalGammaShapeTimes100 = extractInt(
            weatherSec,
            "arrival_gamma_shape_times_100",
            m_weatherArrivalGammaShapeTimes100);
        m_weatherArrivalGammaScaleTimes100 = extractInt(
            weatherSec,
            "arrival_gamma_scale_times_100",
            m_weatherArrivalGammaScaleTimes100);
        m_weatherDurationGammaShapeTimes100 = extractInt(
            weatherSec,
            "duration_gamma_shape_times_100",
            m_weatherDurationGammaShapeTimes100);
        m_weatherDurationGammaScaleTimes100 = extractInt(
            weatherSec,
            "duration_gamma_scale_times_100",
            m_weatherDurationGammaScaleTimes100);
        m_weatherSpeedBlocksPer100Turns = extractInt(
            weatherSec,
            "speed_blocks_per_100_turns",
            m_weatherSpeedBlocksPer100Turns);
        m_weatherEntryCenterWeightTimes100 = extractInt(
            weatherSec,
            "entry_center_weight_times_100",
            m_weatherEntryCenterWeightTimes100);
        m_weatherEntryCornerWeightTimes100 = extractInt(
            weatherSec,
            "entry_corner_weight_times_100",
            m_weatherEntryCornerWeightTimes100);
        m_weatherCoverageMinPercent = extractInt(
            weatherSec, "coverage_min_percent", m_weatherCoverageMinPercent);
        m_weatherCoverageMaxPercent = extractInt(
            weatherSec, "coverage_max_percent", m_weatherCoverageMaxPercent);
        m_weatherAspectRatioMinTimes100 = extractInt(
            weatherSec,
            "aspect_ratio_min_times_100",
            m_weatherAspectRatioMinTimes100);
        m_weatherAspectRatioMaxTimes100 = extractInt(
            weatherSec,
            "aspect_ratio_max_times_100",
            m_weatherAspectRatioMaxTimes100);
        m_weatherShapeNoiseCellSpan = extractInt(
            weatherSec, "shape_noise_cell_span", m_weatherShapeNoiseCellSpan);
        m_weatherShapeNoiseAmplitudePercent = extractInt(
            weatherSec,
            "shape_noise_amplitude_percent",
            m_weatherShapeNoiseAmplitudePercent);
        m_weatherEdgeSoftnessPercent = extractInt(
            weatherSec, "edge_softness_percent", m_weatherEdgeSoftnessPercent);
        m_weatherAlphaBasePercent = extractInt(
            weatherSec, "alpha_base_percent", m_weatherAlphaBasePercent);
        m_weatherAlphaMinPercent = extractInt(
            weatherSec, "alpha_min_percent", m_weatherAlphaMinPercent);
        m_weatherAlphaMaxPercent = extractInt(
            weatherSec, "alpha_max_percent", m_weatherAlphaMaxPercent);
        m_weatherDensityMuTimes100 = extractInt(
            weatherSec, "density_mu_times_100", m_weatherDensityMuTimes100);
        m_weatherDensitySigmaTimes100 = extractInt(
            weatherSec, "density_sigma_times_100", m_weatherDensitySigmaTimes100);

        const std::string directionSec = extractSection(weatherSec, "direction_weights");
        if (!directionSec.empty()) {
            m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::North)] = extractInt(
                directionSec,
                "north",
                m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::North)]);
            m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::South)] = extractInt(
                directionSec,
                "south",
                m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::South)]);
            m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::East)] = extractInt(
                directionSec,
                "east",
                m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::East)]);
            m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::West)] = extractInt(
                directionSec,
                "west",
                m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::West)]);
            m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::NorthEast)] = extractInt(
                directionSec,
                "north_east",
                m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::NorthEast)]);
            m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::NorthWest)] = extractInt(
                directionSec,
                "north_west",
                m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::NorthWest)]);
            m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::SouthEast)] = extractInt(
                directionSec,
                "south_east",
                m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::SouthEast)]);
            m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::SouthWest)] = extractInt(
                directionSec,
                "south_west",
                m_weatherDirectionWeights[weatherDirectionIndex(WeatherDirection::SouthWest)]);
        }
    }

    m_weatherCooldownMinTurns = clampNonNegativeConfigValue(
        "weather.cooldown_min_turns", m_weatherCooldownMinTurns);
    m_weatherArrivalGammaShapeTimes100 = clampNonNegativeConfigValue(
        "weather.arrival_gamma_shape_times_100", m_weatherArrivalGammaShapeTimes100);
    m_weatherArrivalGammaScaleTimes100 = clampNonNegativeConfigValue(
        "weather.arrival_gamma_scale_times_100", m_weatherArrivalGammaScaleTimes100);
    m_weatherDurationGammaShapeTimes100 = clampNonNegativeConfigValue(
        "weather.duration_gamma_shape_times_100", m_weatherDurationGammaShapeTimes100);
    m_weatherDurationGammaScaleTimes100 = clampNonNegativeConfigValue(
        "weather.duration_gamma_scale_times_100", m_weatherDurationGammaScaleTimes100);
    m_weatherSpeedBlocksPer100Turns = clampRangedConfigValue(
        "weather.speed_blocks_per_100_turns", m_weatherSpeedBlocksPer100Turns, 1, 100000);
    for (std::size_t directionIndex = 0; directionIndex < m_weatherDirectionWeights.size(); ++directionIndex) {
        m_weatherDirectionWeights[directionIndex] = clampNonNegativeConfigValue(
            (std::string("weather.direction_weights[") + std::to_string(directionIndex) + "]").c_str(),
            m_weatherDirectionWeights[directionIndex]);
    }
    if (std::all_of(m_weatherDirectionWeights.begin(), m_weatherDirectionWeights.end(), [](int weight) {
            return weight <= 0;
        })) {
        m_weatherDirectionWeights.fill(1);
    }
    m_weatherEntryCenterWeightTimes100 = clampNonNegativeConfigValue(
        "weather.entry_center_weight_times_100", m_weatherEntryCenterWeightTimes100);
    m_weatherEntryCornerWeightTimes100 = clampNonNegativeConfigValue(
        "weather.entry_corner_weight_times_100", m_weatherEntryCornerWeightTimes100);
    m_weatherCoverageMinPercent = clampRangedConfigValue(
        "weather.coverage_min_percent", m_weatherCoverageMinPercent, 1, 95);
    m_weatherCoverageMaxPercent = clampRangedConfigValue(
        "weather.coverage_max_percent", m_weatherCoverageMaxPercent, 1, 95);
    if (m_weatherCoverageMaxPercent < m_weatherCoverageMinPercent) {
        std::swap(m_weatherCoverageMinPercent, m_weatherCoverageMaxPercent);
    }
    m_weatherAspectRatioMinTimes100 = clampRangedConfigValue(
        "weather.aspect_ratio_min_times_100", m_weatherAspectRatioMinTimes100, 100, 600);
    m_weatherAspectRatioMaxTimes100 = clampRangedConfigValue(
        "weather.aspect_ratio_max_times_100", m_weatherAspectRatioMaxTimes100, 100, 600);
    if (m_weatherAspectRatioMaxTimes100 < m_weatherAspectRatioMinTimes100) {
        std::swap(m_weatherAspectRatioMinTimes100, m_weatherAspectRatioMaxTimes100);
    }
    m_weatherShapeNoiseCellSpan = clampRangedConfigValue(
        "weather.shape_noise_cell_span", m_weatherShapeNoiseCellSpan, 1, 32);
    m_weatherShapeNoiseAmplitudePercent = clampRangedConfigValue(
        "weather.shape_noise_amplitude_percent", m_weatherShapeNoiseAmplitudePercent, 0, 100);
    m_weatherEdgeSoftnessPercent = clampRangedConfigValue(
        "weather.edge_softness_percent", m_weatherEdgeSoftnessPercent, 1, 100);
    m_weatherAlphaBasePercent = clampRangedConfigValue(
        "weather.alpha_base_percent", m_weatherAlphaBasePercent, 1, 100);
    m_weatherAlphaMinPercent = clampRangedConfigValue(
        "weather.alpha_min_percent", m_weatherAlphaMinPercent, 0, 100);
    m_weatherAlphaMaxPercent = clampRangedConfigValue(
        "weather.alpha_max_percent", m_weatherAlphaMaxPercent, 1, 100);
    if (m_weatherAlphaMaxPercent < m_weatherAlphaMinPercent) {
        std::swap(m_weatherAlphaMinPercent, m_weatherAlphaMaxPercent);
    }
    m_weatherDensitySigmaTimes100 = clampRangedConfigValue(
        "weather.density_sigma_times_100", m_weatherDensitySigmaTimes100, 1, 200);

    std::string cheatcodeSec = extractSection(root, "cheatcode");
    if (!cheatcodeSec.empty()) {
        m_cheatcodeEnabled = extractBool(cheatcodeSec, "enabled", m_cheatcodeEnabled);

        const std::string shortcutsSec = extractSection(cheatcodeSec, "shortcuts");
        if (!shortcutsSec.empty()) {
            const std::string weatherShortcut = extractString(shortcutsSec, "weather_fog", "");
            const std::string chestShortcut = extractString(shortcutsSec, "chest_loot", "");
            const std::string infernalShortcut = extractString(shortcutsSec, "infernal_piece", "");

            if (!weatherShortcut.empty()) {
                m_cheatcodeWeatherShortcut = parseShortcutKeyToken(weatherShortcut).value_or(sf::Keyboard::Unknown);
            }
            if (!chestShortcut.empty()) {
                m_cheatcodeChestShortcut = parseShortcutKeyToken(chestShortcut).value_or(sf::Keyboard::Unknown);
            }
            if (!infernalShortcut.empty()) {
                m_cheatcodeInfernalShortcut = parseShortcutKeyToken(infernalShortcut).value_or(sf::Keyboard::Unknown);
            }
        }
    }

    sanitizeCheatcodeShortcut("weather_fog", sf::Keyboard::Num1, m_cheatcodeWeatherShortcut);
    sanitizeCheatcodeShortcut("chest_loot", sf::Keyboard::Num2, m_cheatcodeChestShortcut);
    sanitizeCheatcodeShortcut("infernal_piece", sf::Keyboard::Num3, m_cheatcodeInfernalShortcut);
    sanitizeCheatcodeShortcutSet(
        m_cheatcodeWeatherShortcut,
        m_cheatcodeChestShortcut,
        m_cheatcodeInfernalShortcut);

    alignChunkedStructureDimensions("barracks", BuildingType::Barracks, m_barracksWidth, m_barracksHeight);
    alignChunkedStructureDimensions("church", BuildingType::Church, m_churchWidth, m_churchHeight);
    alignChunkedStructureDimensions("mine", BuildingType::Mine, m_mineWidth, m_mineHeight);
    alignChunkedStructureDimensions("farm", BuildingType::Farm, m_farmWidth, m_farmHeight);
    alignChunkedStructureDimensions("arena", BuildingType::Arena, m_arenaWidth, m_arenaHeight);

    return true;
}

// Getters
int GameConfig::getMapRadius() const { return m_mapRadius; }
int GameConfig::getCellSizePx() const { return m_cellSizePx; }
int GameConfig::getNumMines() const { return m_numMines; }
int GameConfig::getNumFarms() const { return m_numFarms; }
int GameConfig::getMinPublicBuildingDistance() const { return m_minPublicBuildingDistance; }
int GameConfig::getPlayerSpawnZonePercent() const { return m_playerSpawnZonePercent; }
int GameConfig::getAISpawnZonePercent() const { return m_aiSpawnZonePercent; }
int GameConfig::getTerrainNoiseScale() const { return m_terrainNoiseScale; }
int GameConfig::getTerrainOctaves() const { return m_terrainOctaves; }
int GameConfig::getDirtCoveragePercent() const { return m_dirtCoveragePercent; }
int GameConfig::getWaterCoveragePercent() const { return m_waterCoveragePercent; }
int GameConfig::getNumDirtBlobs() const { return m_numDirtBlobs; }
int GameConfig::getDirtBlobMinRadius() const { return m_dirtBlobMinRadius; }
int GameConfig::getDirtBlobMaxRadius() const { return m_dirtBlobMaxRadius; }
int GameConfig::getNumLakes() const { return m_numLakes; }
int GameConfig::getLakeMinRadius() const { return m_lakeMinRadius; }
int GameConfig::getLakeMaxRadius() const { return m_lakeMaxRadius; }

int GameConfig::getStartingGold() const { return m_startingGold; }
int GameConfig::getMineIncomePerCellPerTurn() const { return m_mineIncomePerCellPerTurn; }
int GameConfig::getFarmIncomePerCellPerTurn() const { return m_farmIncomePerCellPerTurn; }
int GameConfig::getBarracksCost() const { return m_barracksCost; }
int GameConfig::getWoodWallCost() const { return m_woodWallCost; }
int GameConfig::getStoneWallCost() const { return m_stoneWallCost; }
int GameConfig::getArenaCost() const { return m_arenaCost; }

int GameConfig::getMovementPointsPerTurn() const { return m_movementPointsPerTurn; }
int GameConfig::getBuildPointsPerTurn() const { return m_buildPointsPerTurn; }

int GameConfig::getMovePointCost(PieceType type) const {
    switch (type) {
        case PieceType::Pawn: return m_pawnMovePointCost;
        case PieceType::Knight: return m_knightMovePointCost;
        case PieceType::Bishop: return m_bishopMovePointCost;
        case PieceType::Rook: return m_rookMovePointCost;
        case PieceType::Queen: return m_queenMovePointCost;
        case PieceType::King: return m_kingMovePointCost;
        default: return 0;
    }
}

int GameConfig::getBuildPointCost(BuildingType type) const {
    switch (type) {
        case BuildingType::Barracks: return m_barracksBuildPointCost;
        case BuildingType::WoodWall: return m_woodWallBuildPointCost;
        case BuildingType::StoneWall: return m_stoneWallBuildPointCost;
        case BuildingType::Bridge: return m_bridgeBuildPointCost;
        case BuildingType::Arena: return m_arenaBuildPointCost;
        default: return 0;
    }
}

int GameConfig::getMoveAllowancePerTurn(PieceType type) const {
    switch (type) {
        case PieceType::Pawn: return m_pawnMoveAllowancePerTurn;
        case PieceType::Knight: return m_knightMoveAllowancePerTurn;
        case PieceType::Bishop: return m_bishopMoveAllowancePerTurn;
        case PieceType::Rook: return m_rookMoveAllowancePerTurn;
        case PieceType::Queen: return m_queenMoveAllowancePerTurn;
        case PieceType::King: return m_kingMoveAllowancePerTurn;
        default: return 0;
    }
}

int GameConfig::getPieceUpkeepCost(PieceType type) const {
    switch (type) {
        case PieceType::Pawn: return m_pawnUpkeepCost;
        case PieceType::Knight: return m_knightUpkeepCost;
        case PieceType::Bishop: return m_bishopUpkeepCost;
        case PieceType::Rook: return m_rookUpkeepCost;
        case PieceType::Queen: return m_queenUpkeepCost;
        case PieceType::King: return m_kingUpkeepCost;
        default: return 0;
    }
}

int GameConfig::getRepairCostPerCell(BuildingType type) const {
    switch (type) {
        case BuildingType::Barracks:
            return m_barracksRepairCostPerCell;

        case BuildingType::Arena:
            return m_arenaRepairCostPerCell;

        case BuildingType::Bridge:
            return m_bridgeRepairCostPerCell;

        default:
            return 0;
    }
}

int GameConfig::getRecruitCost(PieceType type) const {
    switch (type) {
        case PieceType::Pawn: return m_pawnRecruitCost;
        case PieceType::Knight: return m_knightRecruitCost;
        case PieceType::Bishop: return m_bishopRecruitCost;
        case PieceType::Rook: return m_rookRecruitCost;
        default: return 0;
    }
}

int GameConfig::getUpgradeCost(PieceType from, PieceType to) const {
    if (from == PieceType::Pawn && to == PieceType::Knight) return m_upgradePawnToKnightCost;
    if (from == PieceType::Pawn && to == PieceType::Bishop) return m_upgradePawnToBishopCost;
    if ((from == PieceType::Knight || from == PieceType::Bishop) && to == PieceType::Rook)
        return m_upgradeToRookCost;
    return 0;
}

int GameConfig::getProductionTurns(PieceType type) const {
    switch (type) {
        case PieceType::Pawn: return m_pawnTurns;
        case PieceType::Knight: return m_knightTurns;
        case PieceType::Bishop: return m_bishopTurns;
        case PieceType::Rook: return m_rookTurns;
        default: return 0;
    }
}

XPRewardProfile GameConfig::getXPRewardProfile(XPRewardSource source) const {
    return m_xpRewardProfiles[xpRewardSourceIndex(source)];
}

int GameConfig::getKillXP(PieceType victim) const {
    switch (victim) {
        case PieceType::Pawn: return getXPRewardProfile(XPRewardSource::KillPawn).mean;
        case PieceType::Knight: return getXPRewardProfile(XPRewardSource::KillKnight).mean;
        case PieceType::Bishop: return getXPRewardProfile(XPRewardSource::KillBishop).mean;
        case PieceType::Rook: return getXPRewardProfile(XPRewardSource::KillRook).mean;
        case PieceType::Queen: return getXPRewardProfile(XPRewardSource::KillQueen).mean;
        default: return 0;
    }
}

int GameConfig::getDestroyBlockXP() const { return getXPRewardProfile(XPRewardSource::DestroyBlock).mean; }
int GameConfig::getArenaXPPerTurn() const { return getXPRewardProfile(XPRewardSource::ArenaPerTurn).mean; }
int GameConfig::getXPThresholdPawnToKnightOrBishop() const { return m_thresholdPawnToKnightOrBishop; }
int GameConfig::getXPThresholdToRook() const { return m_thresholdToRook; }

int GameConfig::getWoodWallHP() const { return m_woodWallHP; }
int GameConfig::getStoneWallHP() const { return m_stoneWallHP; }
int GameConfig::getBarracksCellHP() const { return m_barracksCellHP; }
int GameConfig::getDestroyedCellsRequired(BuildingType type) const {
    switch (type) {
        case BuildingType::WoodWall:
            return m_woodWallDestroyedCellsRequired;
        case BuildingType::StoneWall:
            return m_stoneWallDestroyedCellsRequired;
        case BuildingType::Barracks:
            return m_barracksDestroyedCellsRequired;
        case BuildingType::Bridge:
            return m_bridgeDestroyedCellsRequired;
        case BuildingType::Arena:
            return m_arenaDestroyedCellsRequired;
        case BuildingType::Church:
        case BuildingType::Mine:
        case BuildingType::Farm:
            return 1;
    }

    return 1;
}
int GameConfig::getGlobalMaxRange() const { return m_globalMaxRange; }

int GameConfig::getBuildingWidth(BuildingType type) const {
    switch (type) {
        case BuildingType::Barracks: return m_barracksWidth;
        case BuildingType::Church: return m_churchWidth;
        case BuildingType::Mine: return m_mineWidth;
        case BuildingType::Farm: return m_farmWidth;
        case BuildingType::Arena: return m_arenaWidth;
        case BuildingType::WoodWall: return 1;
        case BuildingType::StoneWall: return 1;
    }
    return 1;
}

int GameConfig::getBuildingHeight(BuildingType type) const {
    switch (type) {
        case BuildingType::Barracks: return m_barracksHeight;
        case BuildingType::Church: return m_churchHeight;
        case BuildingType::Mine: return m_mineHeight;
        case BuildingType::Farm: return m_farmHeight;
        case BuildingType::Arena: return m_arenaHeight;
        case BuildingType::WoodWall: return 1;
        case BuildingType::StoneWall: return 1;
    }
    return 1;
}

int GameConfig::getChestMinSpawnTurn() const { return m_chestMinSpawnTurn; }
int GameConfig::getChestRespawnCooldownTurns() const { return m_chestRespawnCooldownTurns; }
int GameConfig::getChestSpawnRetryTurns() const { return m_chestSpawnRetryTurns; }
int GameConfig::getChestWeibullShapeTimes100() const { return m_chestWeibullShapeTimes100; }
int GameConfig::getChestWeibullScaleTurns() const { return m_chestWeibullScaleTurns; }
int GameConfig::getChestMinDistanceFromKings() const { return m_chestMinDistanceFromKings; }
int GameConfig::getChestGoldRewardAmount() const { return m_chestGoldRewardAmount; }
int GameConfig::getChestMovementBonusAmount() const { return m_chestMovementBonusAmount; }
int GameConfig::getChestBuildBonusAmount() const { return m_chestBuildBonusAmount; }
int GameConfig::getChestLateGameTurn() const { return m_chestLateGameTurn; }
int GameConfig::getChestEarlyGoldWeight() const { return m_chestEarlyGoldWeight; }
int GameConfig::getChestEarlyMovementBonusWeight() const { return m_chestEarlyMovementBonusWeight; }
int GameConfig::getChestEarlyBuildBonusWeight() const { return m_chestEarlyBuildBonusWeight; }
int GameConfig::getChestLateGoldWeight() const { return m_chestLateGoldWeight; }
int GameConfig::getChestLateMovementBonusWeight() const { return m_chestLateMovementBonusWeight; }
int GameConfig::getChestLateBuildBonusWeight() const { return m_chestLateBuildBonusWeight; }

int GameConfig::getInfernalMinSpawnTurn() const { return m_infernalMinSpawnTurn; }
int GameConfig::getInfernalRespawnCooldownTurns() const { return m_infernalRespawnCooldownTurns; }
int GameConfig::getInfernalSpawnRetryTurns() const { return m_infernalSpawnRetryTurns; }
int GameConfig::getInfernalPoissonLambdaBaseTimes1000() const { return m_infernalPoissonLambdaBaseTimes1000; }
int GameConfig::getInfernalPoissonLambdaPerDebtTimes1000() const { return m_infernalPoissonLambdaPerDebtTimes1000; }
int GameConfig::getInfernalPoissonLambdaCapTimes1000() const { return m_infernalPoissonLambdaCapTimes1000; }
int GameConfig::getInfernalBloodDebtDecayPercent() const { return m_infernalBloodDebtDecayPercent; }

int GameConfig::getInfernalBloodDebtForCapturedPiece(PieceType victim) const {
    switch (victim) {
        case PieceType::Pawn:
            return m_infernalDebtPawn;
        case PieceType::Knight:
            return m_infernalDebtKnight;
        case PieceType::Bishop:
            return m_infernalDebtBishop;
        case PieceType::Rook:
            return m_infernalDebtRook;
        case PieceType::Queen:
            return m_infernalDebtQueen;
        case PieceType::King:
            return 0;
    }

    return 0;
}

int GameConfig::getInfernalBloodDebtForStructureDamage() const {
    return m_infernalDebtStructureDamage;
}

int GameConfig::getInfernalTargetWeight(PieceType type) const {
    switch (type) {
        case PieceType::Pawn:
            return m_infernalTargetWeightPawn;
        case PieceType::Knight:
            return m_infernalTargetWeightKnight;
        case PieceType::Bishop:
            return m_infernalTargetWeightBishop;
        case PieceType::Rook:
            return m_infernalTargetWeightRook;
        case PieceType::Queen:
            return m_infernalTargetWeightQueen;
        case PieceType::King:
            return 0;
    }

    return 0;
}

int GameConfig::getInfernalSearchingRandomMoveChanceTimes1000() const {
    return m_infernalSearchingRandomMoveChanceTimes1000;
}

int GameConfig::getWeatherCooldownMinTurns() const { return m_weatherCooldownMinTurns; }
int GameConfig::getWeatherArrivalGammaShapeTimes100() const { return m_weatherArrivalGammaShapeTimes100; }
int GameConfig::getWeatherArrivalGammaScaleTimes100() const { return m_weatherArrivalGammaScaleTimes100; }
int GameConfig::getWeatherDurationGammaShapeTimes100() const { return m_weatherDurationGammaShapeTimes100; }
int GameConfig::getWeatherDurationGammaScaleTimes100() const { return m_weatherDurationGammaScaleTimes100; }
int GameConfig::getWeatherSpeedBlocksPer100Turns() const { return m_weatherSpeedBlocksPer100Turns; }
std::array<int, kNumWeatherDirections> GameConfig::getWeatherDirectionWeights() const {
    return m_weatherDirectionWeights;
}
int GameConfig::getWeatherEntryCenterWeightTimes100() const { return m_weatherEntryCenterWeightTimes100; }
int GameConfig::getWeatherEntryCornerWeightTimes100() const { return m_weatherEntryCornerWeightTimes100; }
int GameConfig::getWeatherCoverageMinPercent() const { return m_weatherCoverageMinPercent; }
int GameConfig::getWeatherCoverageMaxPercent() const { return m_weatherCoverageMaxPercent; }
int GameConfig::getWeatherAspectRatioMinTimes100() const { return m_weatherAspectRatioMinTimes100; }
int GameConfig::getWeatherAspectRatioMaxTimes100() const { return m_weatherAspectRatioMaxTimes100; }
int GameConfig::getWeatherShapeNoiseCellSpan() const { return m_weatherShapeNoiseCellSpan; }
int GameConfig::getWeatherShapeNoiseAmplitudePercent() const { return m_weatherShapeNoiseAmplitudePercent; }
int GameConfig::getWeatherEdgeSoftnessPercent() const { return m_weatherEdgeSoftnessPercent; }
int GameConfig::getWeatherAlphaBasePercent() const { return m_weatherAlphaBasePercent; }
int GameConfig::getWeatherAlphaMinPercent() const { return m_weatherAlphaMinPercent; }
int GameConfig::getWeatherAlphaMaxPercent() const { return m_weatherAlphaMaxPercent; }
int GameConfig::getWeatherDensityMuTimes100() const { return m_weatherDensityMuTimes100; }
int GameConfig::getWeatherDensitySigmaTimes100() const { return m_weatherDensitySigmaTimes100; }

bool GameConfig::isCheatcodeEnabled() const { return m_cheatcodeEnabled; }
sf::Keyboard::Key GameConfig::getCheatcodeWeatherShortcut() const { return m_cheatcodeWeatherShortcut; }
sf::Keyboard::Key GameConfig::getCheatcodeChestShortcut() const { return m_cheatcodeChestShortcut; }
sf::Keyboard::Key GameConfig::getCheatcodeInfernalShortcut() const { return m_cheatcodeInfernalShortcut; }
