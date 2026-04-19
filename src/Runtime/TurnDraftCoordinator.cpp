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
        && state.hasPendingCommands;
}

void TurnDraftCoordinator::invalidate(TurnDraft& turnDraft, std::uint64_t& lastRevision) {
    turnDraft.clear();
    lastRevision = 0;
}

void TurnDraftCoordinator::synchronizeDraft(bool shouldUseDraft,
                                            std::uint64_t revision,
                                            const std::vector<TurnCommand>& commands,
                                            GameEngine& engine,
                                            TurnDraft& turnDraft,
                                            std::uint64_t& lastRevision,
                                            const GameConfig& config,
                                            const TurnDraftSynchronizationCallbacks& callbacks) {
    if (!shouldUseDraft) {
        if (turnDraft.isValid()) {
            const InputSelectionBookmark selectionBookmark = captureSelectionBookmark(callbacks);
            turnDraft.clear();
            lastRevision = revision;
            reconcileSelectionBookmark(callbacks, selectionBookmark);
        } else {
            lastRevision = revision;
        }
        return;
    }

    if (turnDraft.isValid() && lastRevision == revision) {
        return;
    }

    const InputSelectionBookmark selectionBookmark = captureSelectionBookmark(callbacks);
    std::string errorMessage;
    if (!turnDraft.rebuild(engine,
                           config,
                           commands,
                           &errorMessage)) {
        turnDraft.clear();
        lastRevision = revision;
        reconcileSelectionBookmark(callbacks, selectionBookmark);
        return;
    }

    lastRevision = revision;
    reconcileSelectionBookmark(callbacks, selectionBookmark);
}

void TurnDraftCoordinator::ensureUpToDate(const TurnDraftSynchronizationContext& context) {
    synchronizeDraft(shouldUseTurnDraft(context.runtimeState),
                     context.engine.turnSystem().getPendingStateRevision(),
                     context.engine.turnSystem().getPendingCommands(),
                     context.engine,
                     context.turnDraft,
                     context.lastRevision,
                     context.config,
                     context.callbacks);
}