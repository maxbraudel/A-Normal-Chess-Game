#pragma once

#include <array>
#include <cstdint>
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

inline constexpr int kMinMultiplayerPort = 1;
inline constexpr int kMaxMultiplayerPort = 65535;
inline constexpr std::uint32_t kCurrentMultiplayerProtocolVersion = 1;

struct MultiplayerConfig {
    bool enabled = false;
    int port = 0;
    std::string passwordHash;
    std::string passwordSalt;
    std::uint32_t protocolVersion = kCurrentMultiplayerProtocolVersion;
};

inline bool isValidMultiplayerPort(int port) {
    return port >= kMinMultiplayerPort && port <= kMaxMultiplayerPort;
}

struct KingdomParticipantConfig {
    KingdomId kingdom = KingdomId::White;
    ControllerType controller = ControllerType::Human;
    std::string participantName = "Player";
};

inline KingdomParticipantConfig makeKingdomParticipantConfig(KingdomId kingdom,
                                                             ControllerType controller,
                                                             const std::string& participantName) {
    return KingdomParticipantConfig{kingdom, controller, participantName};
}

inline std::array<KingdomParticipantConfig, kNumKingdoms> defaultKingdomParticipants(GameMode mode) {
    switch (mode) {
        case GameMode::HumanVsAI:
            return {
                makeKingdomParticipantConfig(KingdomId::White, ControllerType::Human, "Player"),
                makeKingdomParticipantConfig(KingdomId::Black, ControllerType::AI, "AI")
            };
        case GameMode::HumanVsHuman:
            return {
                makeKingdomParticipantConfig(KingdomId::White, ControllerType::Human, "Player 1"),
                makeKingdomParticipantConfig(KingdomId::Black, ControllerType::Human, "Player 2")
            };
        case GameMode::AIvsAI:
            return {
                makeKingdomParticipantConfig(KingdomId::White, ControllerType::AI, "AI 1"),
                makeKingdomParticipantConfig(KingdomId::Black, ControllerType::AI, "AI 2")
            };
    }

    return defaultKingdomParticipants(GameMode::HumanVsAI);
}

struct GameSessionConfig {
    std::string saveName;
    std::uint32_t worldSeed = 0;
    std::array<KingdomParticipantConfig, kNumKingdoms> kingdoms =
        defaultKingdomParticipants(GameMode::HumanVsAI);
    MultiplayerConfig multiplayer{};
};

struct SaveSummary {
    std::string saveName;
    std::array<KingdomParticipantConfig, kNumKingdoms> kingdoms =
        defaultKingdomParticipants(GameMode::HumanVsAI);
    MultiplayerConfig multiplayer{};
};

inline GameSessionConfig makeDefaultGameSessionConfig(GameMode mode,
                                                      const std::string& saveName = {}) {
    GameSessionConfig session;
    session.saveName = saveName;
    session.kingdoms = defaultKingdomParticipants(mode);
    return session;
}

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

inline bool isValidControllerType(ControllerType type) {
    return type == ControllerType::Human || type == ControllerType::AI;
}

inline const KingdomParticipantConfig& kingdomParticipantConfig(
    const std::array<KingdomParticipantConfig, kNumKingdoms>& participants,
    KingdomId kingdom) {
    return participants[kingdomIndex(kingdom)];
}

inline KingdomParticipantConfig& kingdomParticipantConfig(
    std::array<KingdomParticipantConfig, kNumKingdoms>& participants,
    KingdomId kingdom) {
    return participants[kingdomIndex(kingdom)];
}

inline std::array<ControllerType, kNumKingdoms> controllersFromParticipants(
    const std::array<KingdomParticipantConfig, kNumKingdoms>& participants) {
    return {
        kingdomParticipantConfig(participants, KingdomId::White).controller,
        kingdomParticipantConfig(participants, KingdomId::Black).controller
    };
}

inline bool supportsMultiplayerParticipants(
    const std::array<KingdomParticipantConfig, kNumKingdoms>& participants) {
    const auto controllers = controllersFromParticipants(participants);
    return controllers[0] == ControllerType::Human
        && controllers[1] == ControllerType::Human;
}

inline std::array<std::string, kNumKingdoms> participantNamesFromParticipants(
    const std::array<KingdomParticipantConfig, kNumKingdoms>& participants) {
    return {
        kingdomParticipantConfig(participants, KingdomId::White).participantName,
        kingdomParticipantConfig(participants, KingdomId::Black).participantName
    };
}

inline GameMode gameModeFromControllers(const std::array<ControllerType, kNumKingdoms>& controllers) {
    if (controllers[0] == ControllerType::Human && controllers[1] == ControllerType::Human)
        return GameMode::HumanVsHuman;
    if (controllers[0] == ControllerType::AI && controllers[1] == ControllerType::AI)
        return GameMode::AIvsAI;
    return GameMode::HumanVsAI;
}

inline GameMode gameModeFromParticipants(
    const std::array<KingdomParticipantConfig, kNumKingdoms>& participants) {
    return gameModeFromControllers(controllersFromParticipants(participants));
}

inline GameMode gameModeFromSession(const GameSessionConfig& session) {
    return gameModeFromParticipants(session.kingdoms);
}

inline std::array<ControllerType, kNumKingdoms> controllersForGameMode(GameMode mode) {
    return controllersFromParticipants(defaultKingdomParticipants(mode));
}

inline std::array<std::string, kNumKingdoms> defaultParticipantNames(GameMode mode) {
    return participantNamesFromParticipants(defaultKingdomParticipants(mode));
}

inline std::string kingdomLabel(KingdomId kingdom) {
    return (kingdom == KingdomId::White) ? "White Kingdom" : "Black Kingdom";
}

inline std::string participantRoleLabel(
    const std::array<KingdomParticipantConfig, kNumKingdoms>& participants,
    KingdomId kingdom) {
    const auto controllers = controllersFromParticipants(participants);
    const ControllerType controller = kingdomParticipantConfig(participants, kingdom).controller;

    if (controllers[0] == ControllerType::AI && controllers[1] == ControllerType::AI) {
        return (kingdom == KingdomId::White) ? "AI 1" : "AI 2";
    }
    if (controllers[0] == ControllerType::Human && controllers[1] == ControllerType::Human) {
        return (kingdom == KingdomId::White) ? "Player 1" : "Player 2";
    }
    return (controller == ControllerType::Human) ? "Human" : "AI";
}

inline std::string kingdomAssignmentLabel(
    const std::array<KingdomParticipantConfig, kNumKingdoms>& participants,
    KingdomId kingdom) {
    return participantRoleLabel(participants, kingdom) + " - " + kingdomLabel(kingdom);
}

inline std::string participantNamePrompt(
    const std::array<KingdomParticipantConfig, kNumKingdoms>& participants,
    KingdomId kingdom) {
    return participantRoleLabel(participants, kingdom) + " name";
}

inline const std::string& participantNameFor(const GameSessionConfig& session, KingdomId kingdom) {
    return kingdomParticipantConfig(session.kingdoms, kingdom).participantName;
}

inline ControllerType controllerFor(const GameSessionConfig& session, KingdomId kingdom) {
    return kingdomParticipantConfig(session.kingdoms, kingdom).controller;
}