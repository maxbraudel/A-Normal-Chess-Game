#include "Config/GameConfig.hpp"

#include "Buildings/StructureChunkRegistry.hpp"

#include <iostream>
#include <algorithm>

namespace {

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

    m_killPawn = 20;
    m_killKnight = 50;
    m_killBishop = 50;
    m_killRook = 100;
    m_killQueen = 300;
    m_destroyBlock = 10;
    m_arenaPerTurn = 10;
    m_thresholdPawnToKnightOrBishop = 100;
    m_thresholdToRook = 300;

    m_woodWallHP = 1;
    m_stoneWallHP = 3;
    m_barracksCellHP = 1;
    m_globalMaxRange = 8;

    m_barracksWidth = 4; m_barracksHeight = 3;
    m_churchWidth = 4; m_churchHeight = 3;
    m_mineWidth = 6; m_mineHeight = 6;
    m_farmWidth = 6; m_farmHeight = 4;
    m_arenaWidth = 4; m_arenaHeight = 4;

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

bool GameConfig::loadFromFile(const std::string& filepath) {
    std::string json = readFile(filepath);
    if (json.empty()) {
        std::cerr << "GameConfig: Could not load " << filepath << ", using defaults.\n";
        return false;
    }

    std::string mapSec = extractSection(json, "map");
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

    std::string econSec = extractSection(json, "economy");
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

    std::string turnPointSec = extractSection(json, "turn_points");
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

    std::string prodSec = extractSection(json, "production");
    if (!prodSec.empty()) {
        m_pawnTurns = extractInt(prodSec, "pawn_turns", m_pawnTurns);
        m_knightTurns = extractInt(prodSec, "knight_turns", m_knightTurns);
        m_bishopTurns = extractInt(prodSec, "bishop_turns", m_bishopTurns);
        m_rookTurns = extractInt(prodSec, "rook_turns", m_rookTurns);
    }

    std::string xpSec = extractSection(json, "xp");
    if (!xpSec.empty()) {
        m_killPawn = extractInt(xpSec, "kill_pawn", m_killPawn);
        m_killKnight = extractInt(xpSec, "kill_knight", m_killKnight);
        m_killBishop = extractInt(xpSec, "kill_bishop", m_killBishop);
        m_killRook = extractInt(xpSec, "kill_rook", m_killRook);
        m_killQueen = extractInt(xpSec, "kill_queen", m_killQueen);
        m_destroyBlock = extractInt(xpSec, "destroy_block", m_destroyBlock);
        m_arenaPerTurn = extractInt(xpSec, "arena_per_turn", m_arenaPerTurn);
        m_thresholdPawnToKnightOrBishop = extractInt(xpSec, "threshold_pawn_to_knight_or_bishop", m_thresholdPawnToKnightOrBishop);
        m_thresholdToRook = extractInt(xpSec, "threshold_to_rook", m_thresholdToRook);
    }

    std::string combatSec = extractSection(json, "combat");
    if (!combatSec.empty()) {
        m_woodWallHP = extractInt(combatSec, "wood_wall_hp", m_woodWallHP);
        m_stoneWallHP = extractInt(combatSec, "stone_wall_hp", m_stoneWallHP);
        m_barracksCellHP = extractInt(combatSec, "barracks_cell_hp", m_barracksCellHP);
        m_globalMaxRange = extractInt(combatSec, "global_max_range", m_globalMaxRange);
    }

    std::string buildSec = extractSection(json, "buildings");
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

int GameConfig::getKillXP(PieceType victim) const {
    switch (victim) {
        case PieceType::Pawn: return m_killPawn;
        case PieceType::Knight: return m_killKnight;
        case PieceType::Bishop: return m_killBishop;
        case PieceType::Rook: return m_killRook;
        case PieceType::Queen: return m_killQueen;
        default: return 0;
    }
}

int GameConfig::getDestroyBlockXP() const { return m_destroyBlock; }
int GameConfig::getArenaXPPerTurn() const { return m_arenaPerTurn; }
int GameConfig::getXPThresholdPawnToKnightOrBishop() const { return m_thresholdPawnToKnightOrBishop; }
int GameConfig::getXPThresholdToRook() const { return m_thresholdToRook; }

int GameConfig::getWoodWallHP() const { return m_woodWallHP; }
int GameConfig::getStoneWallHP() const { return m_stoneWallHP; }
int GameConfig::getBarracksCellHP() const { return m_barracksCellHP; }
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
