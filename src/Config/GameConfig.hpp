#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include "Buildings/BuildingType.hpp"
#include "Units/PieceType.hpp"

class GameConfig {
public:
    GameConfig();
    bool loadFromFile(const std::string& filepath);

    // Map
    int getMapRadius() const;
    int getCellSizePx() const;
    int getNumMines() const;
    int getNumFarms() const;
    int getMinPublicBuildingDistance() const;
    int getPlayerSpawnZonePercent() const;
    int getAISpawnZonePercent() const;
    int getTerrainNoiseScale() const;
    int getTerrainOctaves() const;
    int getDirtCoveragePercent() const;
    int getWaterCoveragePercent() const;
    int getNumDirtBlobs() const;
    int getDirtBlobMinRadius() const;
    int getDirtBlobMaxRadius() const;
    int getNumLakes() const;
    int getLakeMinRadius() const;
    int getLakeMaxRadius() const;

    // Economy
    int getStartingGold() const;
    int getMineIncomePerCellPerTurn() const;
    int getFarmIncomePerCellPerTurn() const;
    int getBarracksCost() const;
    int getWoodWallCost() const;
    int getStoneWallCost() const;
    int getArenaCost() const;
    int getRepairCostPerCell(BuildingType type) const;
    int getMovementPointsPerTurn() const;
    int getBuildPointsPerTurn() const;
    int getMovePointCost(PieceType type) const;
    int getBuildPointCost(BuildingType type) const;
    int getMoveAllowancePerTurn(PieceType type) const;
    int getPieceUpkeepCost(PieceType type) const;
    int getRecruitCost(PieceType type) const;
    int getUpgradeCost(PieceType from, PieceType to) const;

    // Production
    int getProductionTurns(PieceType type) const;

    // XP
    int getKillXP(PieceType victim) const;
    int getDestroyBlockXP() const;
    int getArenaXPPerTurn() const;
    int getXPThresholdPawnToKnightOrBishop() const;
    int getXPThresholdToRook() const;

    // Combat
    int getWoodWallHP() const;
    int getStoneWallHP() const;
    int getBarracksCellHP() const;
    int getGlobalMaxRange() const;

    // Buildings
    int getBuildingWidth(BuildingType type) const;
    int getBuildingHeight(BuildingType type) const;

    // Chests
    int getChestMinSpawnTurn() const;
    int getChestRespawnCooldownTurns() const;
    int getChestSpawnRetryTurns() const;
    int getChestWeibullShapeTimes100() const;
    int getChestWeibullScaleTurns() const;
    int getChestMinDistanceFromKings() const;
    int getChestGoldRewardAmount() const;
    int getChestMovementBonusAmount() const;
    int getChestBuildBonusAmount() const;
    int getChestLateGameTurn() const;
    int getChestEarlyGoldWeight() const;
    int getChestEarlyMovementBonusWeight() const;
    int getChestEarlyBuildBonusWeight() const;
    int getChestLateGoldWeight() const;
    int getChestLateMovementBonusWeight() const;
    int getChestLateBuildBonusWeight() const;

private:
    // Map params
    int m_mapRadius;
    int m_cellSizePx;
    int m_numMines;
    int m_numFarms;
    int m_minPublicBuildingDistance;
    int m_playerSpawnZonePercent;
    int m_aiSpawnZonePercent;
    int m_terrainNoiseScale;
    int m_terrainOctaves;
    int m_dirtCoveragePercent;
    int m_waterCoveragePercent;
    int m_numDirtBlobs;
    int m_dirtBlobMinRadius;
    int m_dirtBlobMaxRadius;
    int m_numLakes;
    int m_lakeMinRadius;
    int m_lakeMaxRadius;

    // Economy
    int m_startingGold;
    int m_mineIncomePerCellPerTurn;
    int m_farmIncomePerCellPerTurn;
    int m_barracksCost;
    int m_woodWallCost;
    int m_stoneWallCost;
    int m_arenaCost;
    int m_barracksRepairCostPerCell;
    int m_arenaRepairCostPerCell;
    int m_bridgeRepairCostPerCell;
    int m_movementPointsPerTurn;
    int m_buildPointsPerTurn;
    int m_pawnMovePointCost;
    int m_knightMovePointCost;
    int m_bishopMovePointCost;
    int m_rookMovePointCost;
    int m_queenMovePointCost;
    int m_kingMovePointCost;
    int m_barracksBuildPointCost;
    int m_woodWallBuildPointCost;
    int m_stoneWallBuildPointCost;
    int m_bridgeBuildPointCost;
    int m_arenaBuildPointCost;
    int m_pawnMoveAllowancePerTurn;
    int m_knightMoveAllowancePerTurn;
    int m_bishopMoveAllowancePerTurn;
    int m_rookMoveAllowancePerTurn;
    int m_queenMoveAllowancePerTurn;
    int m_kingMoveAllowancePerTurn;
    int m_pawnRecruitCost;
    int m_knightRecruitCost;
    int m_bishopRecruitCost;
    int m_rookRecruitCost;
    int m_pawnUpkeepCost;
    int m_knightUpkeepCost;
    int m_bishopUpkeepCost;
    int m_rookUpkeepCost;
    int m_queenUpkeepCost;
    int m_kingUpkeepCost;
    int m_upgradePawnToKnightCost;
    int m_upgradePawnToBishopCost;
    int m_upgradeToRookCost;

    // Production
    int m_pawnTurns;
    int m_knightTurns;
    int m_bishopTurns;
    int m_rookTurns;

    // XP
    int m_killPawn;
    int m_killKnight;
    int m_killBishop;
    int m_killRook;
    int m_killQueen;
    int m_destroyBlock;
    int m_arenaPerTurn;
    int m_thresholdPawnToKnightOrBishop;
    int m_thresholdToRook;

    // Combat
    int m_woodWallHP;
    int m_stoneWallHP;
    int m_barracksCellHP;
    int m_globalMaxRange;

    // Buildings  
    int m_barracksWidth, m_barracksHeight;
    int m_churchWidth, m_churchHeight;
    int m_mineWidth, m_mineHeight;
    int m_farmWidth, m_farmHeight;
    int m_arenaWidth, m_arenaHeight;

    // Chests
    int m_chestMinSpawnTurn;
    int m_chestRespawnCooldownTurns;
    int m_chestSpawnRetryTurns;
    int m_chestWeibullShapeTimes100;
    int m_chestWeibullScaleTurns;
    int m_chestMinDistanceFromKings;
    int m_chestGoldRewardAmount;
    int m_chestMovementBonusAmount;
    int m_chestBuildBonusAmount;
    int m_chestLateGameTurn;
    int m_chestEarlyGoldWeight;
    int m_chestEarlyMovementBonusWeight;
    int m_chestEarlyBuildBonusWeight;
    int m_chestLateGoldWeight;
    int m_chestLateMovementBonusWeight;
    int m_chestLateBuildBonusWeight;

    void setDefaults();
    
    // Simple JSON parsing helpers
    static std::string readFile(const std::string& path);
    static int extractInt(const std::string& json, const std::string& key, int defaultVal);
    static std::string extractSection(const std::string& json, const std::string& key);
};
