#include "Runtime/TurnDraftCoordinator.hpp"

#include <string>

#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Core/TurnDraft.hpp"

namespace {

InputSelectionBookmark captureSelectionBookmark(const TurnDraftSynchronizationCallbacks& callbacks) {
    if (callbacks.captureSelectionBookmark) {
        return callbacks.captureSelectionBookmark();
    }

    return InputSelectionBookmark{};
}

void reconcileSelectionBookmark(const TurnDraftSynchronizationCallbacks& callbacks,
                                const InputSelectionBookmark& bookmark) {
    if (callbacks.reconcileSelectionBookmark) {
        callbacks.reconcileSelectionBookmark(bookmark);
    }
}

} // namespace

bool TurnDraftCoordinator::shouldUseTurnDraft(const TurnDraftRuntimeState& state) {
    return (state.gameState == GameState::Playing
            || state.gameState == GameState::Paused
            || state.gameState == GameState::GameOver)
        && state.isLocalPlayerTurn
        && !state.waitingForRemoteTurnResult
        && state.hasPendingCommands;
}

void TurnDraftCoordinator::invalidate(TurnDraft& turnDraft, std::uint64_t& lastRevision) {
    turnDraft.clear();
    lastRevision = 0;
}

void TurnDraftCoordinator::ensureUpToDate(const TurnDraftSynchronizationContext& context) {
    const std::uint64_t revision = context.engine.turnSystem().getPendingStateRevision();
    if (!shouldUseTurnDraft(context.runtimeState)) {
        if (context.turnDraft.isValid()) {
            const InputSelectionBookmark selectionBookmark = captureSelectionBookmark(context.callbacks);
            context.turnDraft.clear();
            context.lastRevision = revision;
            reconcileSelectionBookmark(context.callbacks, selectionBookmark);
        } else {
            context.lastRevision = revision;
        }
        return;
    }

    if (context.turnDraft.isValid() && context.lastRevision == revision) {
        return;
    }

    const InputSelectionBookmark selectionBookmark = captureSelectionBookmark(context.callbacks);
    std::string errorMessage;
    if (!context.turnDraft.rebuild(context.engine,
                                   context.config,
                                   context.engine.turnSystem().getPendingCommands(),
                                   &errorMessage)) {
        context.turnDraft.clear();
        context.lastRevision = revision;
        reconcileSelectionBookmark(context.callbacks, selectionBookmark);
        return;
    }

    context.lastRevision = revision;
    reconcileSelectionBookmark(context.callbacks, selectionBookmark);
}