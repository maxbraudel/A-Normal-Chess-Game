#pragma once

#include "Core/GameState.hpp"
#include "UI/InGameViewModel.hpp"

class GameConfig;
class GameEngine;
class Kingdom;

int countOccupiedBuildingCells(const Kingdom& kingdom);
InGameViewModel buildInGameViewModel(const GameEngine& engine,
                                     const GameConfig& config,
                                     GameState state,
                                     bool allowCommands,
                                     const InGameHudPresentation& presentation);