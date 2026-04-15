#pragma once

#include <array>
#include <string>

#include "Kingdom/KingdomId.hpp"

enum class ControllerType {
    Human = 0,
    AI = 1
};

enum class GameMode {
    HumanVsAI = 0,
    HumanVsHuman = 1,
    AIvsAI = 2
};

struct GameSessionConfig {
    std::string saveName;
    GameMode mode = GameMode::HumanVsAI;
    std::array<std::string, kNumKingdoms> participantNames{"Player", "AI"};
};

struct SaveSummary {
    std::string saveName;
    GameMode mode = GameMode::HumanVsAI;
    std::array<std::string, kNumKingdoms> participantNames{"Player", "AI"};
};

inline const char* controllerTypeLabel(ControllerType type) {
    switch (type) {
        case ControllerType::Human: return "Human";
        case ControllerType::AI: return "AI";
    }
    return "Unknown";
}

inline const char* gameModeLabel(GameMode mode) {
    switch (mode) {
        case GameMode::HumanVsAI: return "Human vs AI";
        case GameMode::HumanVsHuman: return "Human vs Human";
        case GameMode::AIvsAI: return "AI vs AI";
    }
    return "Unknown";
}

inline std::array<ControllerType, kNumKingdoms> controllersForGameMode(GameMode mode) {
    switch (mode) {
        case GameMode::HumanVsAI:
            return {ControllerType::Human, ControllerType::AI};
        case GameMode::HumanVsHuman:
            return {ControllerType::Human, ControllerType::Human};
        case GameMode::AIvsAI:
            return {ControllerType::AI, ControllerType::AI};
    }
    return {ControllerType::Human, ControllerType::AI};
}

inline GameMode gameModeFromControllers(const std::array<ControllerType, kNumKingdoms>& controllers) {
    if (controllers[0] == ControllerType::Human && controllers[1] == ControllerType::Human)
        return GameMode::HumanVsHuman;
    if (controllers[0] == ControllerType::AI && controllers[1] == ControllerType::AI)
        return GameMode::AIvsAI;
    return GameMode::HumanVsAI;
}

inline std::array<std::string, kNumKingdoms> defaultParticipantNames(GameMode mode) {
    switch (mode) {
        case GameMode::HumanVsAI:
            return {"Player", "AI"};
        case GameMode::HumanVsHuman:
            return {"Player 1", "Player 2"};
        case GameMode::AIvsAI:
            return {"AI 1", "AI 2"};
    }
    return {"Player", "AI"};
}

inline const char* participantPrompt(GameMode mode, KingdomId id) {
    const bool isWhite = (id == KingdomId::White);
    switch (mode) {
        case GameMode::HumanVsAI:
            return isWhite ? "Human player name" : "AI name";
        case GameMode::HumanVsHuman:
            return isWhite ? "Player 1 name" : "Player 2 name";
        case GameMode::AIvsAI:
            return isWhite ? "AI 1 name" : "AI 2 name";
    }
    return isWhite ? "Player 1 name" : "Player 2 name";
}