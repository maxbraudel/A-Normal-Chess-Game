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

private:
    // Map params
    int m_mapRadius;
    int m_cellSizePx;
    int m_numMines;
    int m_numFarms;
    int m_minPublicBuildingDistance;
    int m_playerSpawnZonePercent;
    int m_aiSpawnZonePercent;
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
    int m_pawnRecruitCost;
    int m_knightRecruitCost;
    int m_bishopRecruitCost;
    int m_rookRecruitCost;
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

    void setDefaults();
    
    // Simple JSON parsing helpers
    static std::string readFile(const std::string& path);
    static int extractInt(const std::string& json, const std::string& key, int defaultVal);
    static std::string extractSection(const std::string& json, const std::string& key);
};
