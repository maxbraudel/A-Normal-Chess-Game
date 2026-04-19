#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "Core/GameState.hpp"
#include "Input/InputSelectionBookmark.hpp"

class GameConfig;
class GameEngine;
class TurnDraft;
struct TurnCommand;

struct TurnDraftRuntimeState {
    GameState gameState = GameState::MainMenu;
    bool isLocalPlayerTurn = false;
    bool waitingForRemoteTurnResult = false;
    bool hasPendingCommands = false;
};

struct TurnDraftSynchronizationCallbacks {
    std::function<InputSelectionBookmark()> captureSelectionBookmark;
    std::function<void(const InputSelectionBookmark&)> reconcileSelectionBookmark;
};

struct TurnDraftSynchronizationContext {
    TurnDraftRuntimeState runtimeState;
    GameEngine& engine;
    TurnDraft& turnDraft;
    std::uint64_t& lastRevision;
    const GameConfig& config;
    TurnDraftSynchronizationCallbacks callbacks;
};

class TurnDraftCoordinator {
public:
    static bool shouldUseTurnDraft(const TurnDraftRuntimeState& state);
    static void invalidate(TurnDraft& turnDraft, std::uint64_t& lastRevision);
    static void synchronizeDraft(bool shouldUseDraft,
                                 std::uint64_t revision,
                                 const std::vector<TurnCommand>& commands,
                                 GameEngine& engine,
                                 TurnDraft& turnDraft,
                                 std::uint64_t& lastRevision,
                                 const GameConfig& config,
                                 const TurnDraftSynchronizationCallbacks& callbacks);
    static void ensureUpToDate(const TurnDraftSynchronizationContext& context);
};