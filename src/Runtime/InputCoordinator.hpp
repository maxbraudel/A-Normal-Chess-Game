#pragma once

#include <optional>

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

#include "Core/GameState.hpp"
#include "Core/InteractionPermissions.hpp"

struct InputFrameState {
    GameState gameState = GameState::MainMenu;
    bool overlaysVisible = false;
    bool inGameMenuOpen = false;
    bool cheatcodeEnabled = false;
    sf::Keyboard::Key cheatcodeWeatherShortcut = sf::Keyboard::Unknown;
    sf::Keyboard::Key cheatcodeChestShortcut = sf::Keyboard::Unknown;
    sf::Keyboard::Key cheatcodeInfernalShortcut = sf::Keyboard::Unknown;
    InteractionPermissions permissions{};
};

enum class InputPreGuiActionKind {
    None,
    CloseWindow,
    ResizeWindow,
    ToggleInGameMenu,
    ExportDebugHistory,
    CommitTurn,
    CenterCameraOnPerspective,
    TriggerCheatcodeWeather,
    TriggerCheatcodeChest,
    TriggerCheatcodeInfernal,
    ActivateSelectTool,
    SkipEvent
};

struct InputPreGuiAction {
    InputPreGuiActionKind kind = InputPreGuiActionKind::None;
    sf::Vector2u resizedWindow{0u, 0u};
};

enum class InputPostGuiActionKind {
    SkipEvent,
    DispatchToWorld
};

struct InputPostGuiAction {
    InputPostGuiActionKind kind = InputPostGuiActionKind::SkipEvent;
};

class InputCoordinator {
public:
    static bool isInteractiveGameState(const InputFrameState& state);
    static InputPreGuiAction planPreGuiAction(const sf::Event& event,
                                              const InputFrameState& state);
    static InputPostGuiAction planPostGuiAction(const InputFrameState& state,
                                                bool handledByGui,
                                                bool worldMouseBlocked);
    static bool isBlockedGuiNavigationEvent(const sf::Event& event);
    static std::optional<sf::Vector2i> mouseScreenPositionFromEvent(const sf::Event& event);
};