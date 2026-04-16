#pragma once

#include "Core/InteractionPermissions.hpp"
#include "Units/PieceType.hpp"

class GameConfig;
class GameEngine;
class InputHandler;

class PanelActionCoordinator {
public:
    PanelActionCoordinator(GameEngine& engine,
                           InputHandler& input,
                           const GameConfig& config);

    void cancelQueuedConstructionSelection(const InteractionPermissions& permissions,
                                           int buildingId);
    void handlePieceUpgradeRequest(const InteractionPermissions& permissions,
                                   int pieceId,
                                   PieceType requestedTarget);
    void handlePieceDisbandRequest(const InteractionPermissions& permissions,
                                   int pieceId);
    void handleBarracksProduceRequest(const InteractionPermissions& permissions,
                                      int barracksId,
                                      PieceType requestedType);

private:
    GameEngine& m_engine;
    InputHandler& m_input;
    const GameConfig& m_config;
};