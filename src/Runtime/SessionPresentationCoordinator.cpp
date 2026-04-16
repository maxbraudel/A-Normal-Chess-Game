#include "Runtime/SessionPresentationCoordinator.hpp"

SessionResetPlan SessionPresentationCoordinator::buildAuthoritativeSessionResetPlan() {
    SessionResetPlan plan;
    plan.stopMultiplayer = true;
    plan.discardPendingAiTurn = true;
    plan.clearMovePreview = true;
    plan.activateSelectTool = true;
    plan.resetPendingTurn = true;
    plan.invalidateTurnDraft = true;
    return plan;
}

SessionPresentationPlan SessionPresentationCoordinator::buildSessionPresentationPlan(
    const GameSessionConfig& session,
    bool saveAuthoritativeSession) {
    SessionPresentationPlan plan;
    plan.localPlayerContext = makeLocalPlayerContextForSession(session);
    plan.clearWaitingForRemoteTurnResult = true;
    plan.cameraKingdom = plan.localPlayerContext.perspectiveKingdom;
    plan.nextGameState = GameState::Playing;
    plan.refreshTurnPhase = true;
    plan.showHud = true;
    plan.updateUI = true;
    plan.saveAuthoritativeSession = saveAuthoritativeSession;
    return plan;
}

MainMenuPresentationPlan SessionPresentationCoordinator::buildReturnToMainMenuPlan() {
    MainMenuPresentationPlan plan;
    plan.stopMultiplayer = true;
    plan.discardPendingAiTurn = true;
    plan.clearMovePreview = true;
    plan.activateSelectTool = true;
    plan.resetPendingTurn = true;
    plan.invalidateTurnDraft = true;
    plan.nextGameState = GameState::MainMenu;
    plan.hideGameMenu = true;
    plan.showMainMenu = true;
    return plan;
}