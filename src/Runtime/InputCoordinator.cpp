#include "Runtime/InputCoordinator.hpp"

#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

namespace {

bool isBlockedGameplayShortcutKey(sf::Keyboard::Key key) {
    return key == sf::Keyboard::Escape
        || key == sf::Keyboard::P
        || key == sf::Keyboard::K
        || key == sf::Keyboard::Space;
}

} // namespace

bool InputCoordinator::isInteractiveGameState(const InputFrameState& state) {
    return state.gameState == GameState::Playing
        || state.gameState == GameState::Paused
        || state.gameState == GameState::GameOver;
}

InputPreGuiAction InputCoordinator::planPreGuiAction(const sf::Event& event,
                                                     const InputFrameState& state) {
    if (event.type == sf::Event::Closed) {
        return {InputPreGuiActionKind::CloseWindow, {0u, 0u}};
    }

    if (event.type == sf::Event::Resized) {
        return {
            InputPreGuiActionKind::ResizeWindow,
            sf::Vector2u(event.size.width, event.size.height)
        };
    }

    if (isInteractiveGameState(state)
        && !state.overlaysVisible
        && event.type == sf::Event::KeyPressed
        && isBlockedGameplayShortcutKey(event.key.code)) {
        if (event.key.code == sf::Keyboard::Escape) {
            return {InputPreGuiActionKind::ToggleInGameMenu, {0u, 0u}};
        }

        if (event.key.code == sf::Keyboard::P) {
            return {InputPreGuiActionKind::ExportDebugHistory, {0u, 0u}};
        }

        if (state.inGameMenuOpen) {
            return {InputPreGuiActionKind::SkipEvent, {0u, 0u}};
        }

        if (event.key.code == sf::Keyboard::Space) {
            return {
                state.permissions.canIssueCommands
                    ? InputPreGuiActionKind::CommitTurn
                    : InputPreGuiActionKind::SkipEvent,
                {0u, 0u}
            };
        }

        if (event.key.code == sf::Keyboard::K) {
            return {InputPreGuiActionKind::CenterCameraOnPerspective, {0u, 0u}};
        }
    }

    if (isInteractiveGameState(state)
        && !state.overlaysVisible
        && !state.inGameMenuOpen
        && event.type == sf::Event::MouseButtonPressed
        && event.mouseButton.button == sf::Mouse::Right) {
        return {
            state.permissions.canUseToolbar
                ? InputPreGuiActionKind::ActivateSelectTool
                : InputPreGuiActionKind::SkipEvent,
            {0u, 0u}
        };
    }

    if (isBlockedGuiNavigationEvent(event)) {
        return {InputPreGuiActionKind::SkipEvent, {0u, 0u}};
    }

    return {InputPreGuiActionKind::None, {0u, 0u}};
}

InputPostGuiAction InputCoordinator::planPostGuiAction(const InputFrameState& state,
                                                       bool handledByGui,
                                                       bool worldMouseBlocked) {
    if (!isInteractiveGameState(state)
        || state.overlaysVisible
        || handledByGui
        || worldMouseBlocked
        || state.inGameMenuOpen
        || !state.permissions.canInspectWorld) {
        return {InputPostGuiActionKind::SkipEvent};
    }

    return {InputPostGuiActionKind::DispatchToWorld};
}

bool InputCoordinator::isBlockedGuiNavigationEvent(const sf::Event& event) {
    if (event.type != sf::Event::KeyPressed && event.type != sf::Event::KeyReleased) {
        return false;
    }

    return event.key.code == sf::Keyboard::Tab;
}

std::optional<sf::Vector2i> InputCoordinator::mouseScreenPositionFromEvent(const sf::Event& event) {
    switch (event.type) {
        case sf::Event::MouseMoved:
            return sf::Vector2i(event.mouseMove.x, event.mouseMove.y);
        case sf::Event::MouseButtonPressed:
        case sf::Event::MouseButtonReleased:
            return sf::Vector2i(event.mouseButton.x, event.mouseButton.y);
        case sf::Event::MouseWheelScrolled:
            return sf::Vector2i(event.mouseWheelScroll.x, event.mouseWheelScroll.y);
        default:
            return std::nullopt;
    }
}