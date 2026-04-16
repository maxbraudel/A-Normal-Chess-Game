#include "Runtime/PanelActionCoordinator.hpp"

#include "Buildings/BuildingFactory.hpp"
#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Input/InputHandler.hpp"

PanelActionCoordinator::PanelActionCoordinator(GameEngine& engine,
                                               InputHandler& input,
                                               const GameConfig& config)
    : m_engine(engine)
    , m_input(input)
    , m_config(config) {}

void PanelActionCoordinator::cancelQueuedConstructionSelection(const InteractionPermissions& permissions,
                                                               int buildingId) {
    if (!permissions.canQueueNonMoveActions) {
        return;
    }

    if (m_engine.turnSystem().cancelBuildCommand(buildingId, m_engine.makeTurnValidationContext(m_config))) {
        m_input.clearSelection();
    }
}

void PanelActionCoordinator::handlePieceUpgradeRequest(const InteractionPermissions& permissions,
                                                       int pieceId,
                                                       PieceType requestedTarget) {
    if (!permissions.canQueueNonMoveActions) {
        return;
    }

    Piece* piece = m_engine.activeKingdom().getPieceById(pieceId);
    if (!piece) {
        return;
    }

    TurnSystem& turnSystem = m_engine.turnSystem();
    const TurnCommand* queuedUpgrade = turnSystem.getPendingUpgradeCommand(pieceId);
    TurnCommand previousUpgrade;
    const bool hadQueuedUpgrade = (queuedUpgrade != nullptr);
    const bool cancelOnly = hadQueuedUpgrade && queuedUpgrade->upgradeTarget == requestedTarget;
    const TurnValidationContext turnContext = m_engine.makeTurnValidationContext(m_config);
    if (hadQueuedUpgrade) {
        previousUpgrade = *queuedUpgrade;
        if (!turnSystem.cancelUpgradeCommand(pieceId, turnContext)) {
            return;
        }
        if (cancelOnly) {
            return;
        }
    }

    TurnCommand command;
    command.type = TurnCommand::Upgrade;
    command.upgradePieceId = piece->id;
    command.upgradeTarget = requestedTarget;
    if (!piece->canUpgradeTo(command.upgradeTarget, m_config)) {
        if (hadQueuedUpgrade) {
            turnSystem.queueCommand(previousUpgrade, turnContext);
        }
        return;
    }

    if (!turnSystem.queueCommand(command, turnContext, &m_engine.buildingFactory()) && hadQueuedUpgrade) {
        turnSystem.queueCommand(previousUpgrade, turnContext);
    }
}

void PanelActionCoordinator::handlePieceDisbandRequest(const InteractionPermissions& permissions,
                                                       int pieceId) {
    if (!permissions.canQueueNonMoveActions) {
        return;
    }

    Piece* piece = m_engine.activeKingdom().getPieceById(pieceId);
    if (!piece || piece->type == PieceType::King) {
        return;
    }

    TurnSystem& turnSystem = m_engine.turnSystem();
    const TurnValidationContext turnContext = m_engine.makeTurnValidationContext(m_config);
    if (turnSystem.getPendingDisbandCommand(pieceId)) {
        turnSystem.cancelDisbandCommand(pieceId, turnContext);
        return;
    }

    TurnCommand command;
    command.type = TurnCommand::Disband;
    command.pieceId = pieceId;
    turnSystem.queueCommand(command, turnContext, &m_engine.buildingFactory());
}

void PanelActionCoordinator::handleBarracksProduceRequest(const InteractionPermissions& permissions,
                                                          int barracksId,
                                                          PieceType requestedType) {
    if (!permissions.canQueueNonMoveActions) {
        return;
    }

    TurnSystem& turnSystem = m_engine.turnSystem();
    const TurnCommand* queuedProduction = turnSystem.getPendingProduceCommand(barracksId);
    TurnCommand previousProduction;
    const bool hadQueuedProduction = (queuedProduction != nullptr);
    const bool cancelOnly = hadQueuedProduction && queuedProduction->produceType == requestedType;
    const TurnValidationContext turnContext = m_engine.makeTurnValidationContext(m_config);
    if (hadQueuedProduction) {
        previousProduction = *queuedProduction;
        if (!turnSystem.cancelProduceCommand(barracksId, turnContext)) {
            return;
        }
        if (cancelOnly) {
            return;
        }
    }

    TurnCommand command;
    command.type = TurnCommand::Produce;
    command.barracksId = barracksId;
    command.produceType = requestedType;
    if (!turnSystem.queueCommand(command, turnContext, &m_engine.buildingFactory()) && hadQueuedProduction) {
        turnSystem.queueCommand(previousProduction, turnContext);
    }
}