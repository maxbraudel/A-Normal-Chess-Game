#include "Runtime/SessionMetadataService.hpp"

#include <filesystem>

#include "Core/GameStateValidator.hpp"
#include "Multiplayer/PasswordUtils.hpp"
#include "Save/SaveData.hpp"
#include "Save/SaveManager.hpp"

namespace {

namespace fs = std::filesystem;

bool isValidSaveNameForFileSystem(const std::string& value) {
    return !value.empty()
        && value.find_first_of("<>:\"/\\|?*") == std::string::npos
        && value.find('\n') == std::string::npos
        && value.find('\r') == std::string::npos;
}

bool isValidParticipantNameForMetadata(const std::string& value) {
    return !value.empty()
        && value.find('"') == std::string::npos
        && value.find('\\') == std::string::npos
        && value.find('\n') == std::string::npos
        && value.find('\r') == std::string::npos;
}

void writeError(std::string* errorMessage, const std::string& message) {
    if (errorMessage) {
        *errorMessage = message;
    }
}

std::string buildSavePath(const std::string& savesDirectory, const std::string& saveName) {
    return (fs::path(savesDirectory) / (saveName + ".json")).string();
}

bool validateSessionMetadataStrings(const GameSessionConfig& session, std::string* errorMessage) {
    if (!isValidSaveNameForFileSystem(session.saveName)) {
        writeError(errorMessage, "Save name contains invalid characters.");
        return false;
    }

    if (!isValidParticipantNameForMetadata(participantNameFor(session, KingdomId::White))
        || !isValidParticipantNameForMetadata(participantNameFor(session, KingdomId::Black))) {
        writeError(errorMessage, "Names cannot contain quotes or line breaks.");
        return false;
    }

    return true;
}

bool finalizeMultiplayerConfig(const MultiplayerConfig& requested,
                              const std::string& plaintextPassword,
                              const MultiplayerConfig* existing,
                              MultiplayerConfig& outMultiplayer,
                              std::string* errorMessage) {
    if (!requested.enabled) {
        outMultiplayer = MultiplayerConfig{};
        return true;
    }

    outMultiplayer = requested;
    outMultiplayer.protocolVersion = kCurrentMultiplayerProtocolVersion;

    if (!plaintextPassword.empty()) {
        outMultiplayer.passwordSalt = MultiplayerPasswordUtils::generateSalt();
        outMultiplayer.passwordHash = MultiplayerPasswordUtils::computePasswordDigest(
            plaintextPassword,
            outMultiplayer.passwordSalt);
        return true;
    }

    if (existing != nullptr
        && existing->enabled
        && !existing->passwordHash.empty()
        && !existing->passwordSalt.empty()) {
        outMultiplayer.passwordHash = existing->passwordHash;
        outMultiplayer.passwordSalt = existing->passwordSalt;
        if (existing->protocolVersion != 0u) {
            outMultiplayer.protocolVersion = existing->protocolVersion;
        }
        return true;
    }

    writeError(errorMessage, "A multiplayer password is required.");
    return false;
}

bool finalizeSessionConfig(const SessionFormRequest& request,
                           const MultiplayerConfig* existingMultiplayer,
                           GameSessionConfig& outSession,
                           std::string* errorMessage) {
    outSession = request.session;
    if (!validateSessionMetadataStrings(outSession, errorMessage)) {
        return false;
    }

    MultiplayerConfig finalizedMultiplayer;
    if (!finalizeMultiplayerConfig(outSession.multiplayer,
                                   request.multiplayerPassword,
                                   existingMultiplayer,
                                   finalizedMultiplayer,
                                   errorMessage)) {
        return false;
    }

    outSession.multiplayer = finalizedMultiplayer;
    return GameStateValidator::validateSessionConfig(outSession, errorMessage);
}

} // namespace

bool SessionMetadataService::prepareSessionForStart(const SessionFormRequest& request,
                                                    GameSessionConfig& outSession,
                                                    std::string* errorMessage) {
    return finalizeSessionConfig(request, nullptr, outSession, errorMessage);
}

bool SessionMetadataService::editSavedSession(const SessionFormRequest& request,
                                              SaveManager& saveManager,
                                              const std::string& savesDirectory,
                                              std::string* errorMessage) {
    if (request.originalSaveName.empty()) {
        writeError(errorMessage, "An existing save must be selected for editing.");
        return false;
    }

    const std::string originalPath = buildSavePath(savesDirectory, request.originalSaveName);
    SaveData data;
    if (!saveManager.load(originalPath, data)) {
        writeError(errorMessage, "Failed to load the selected save.");
        return false;
    }

    GameSessionConfig finalizedSession;
    if (!finalizeSessionConfig(request, &data.multiplayer, finalizedSession, errorMessage)) {
        return false;
    }

    data.gameName = finalizedSession.saveName;
    data.sessionKingdoms = finalizedSession.kingdoms;
    data.multiplayer = finalizedSession.multiplayer;
    data.tacticalGridEnabled = finalizedSession.tacticalGridEnabled;

    if (!GameStateValidator::validateSaveData(data, errorMessage)) {
        return false;
    }

    const std::string targetPath = buildSavePath(savesDirectory, finalizedSession.saveName);
    if (targetPath != originalPath && fs::exists(targetPath)) {
        writeError(errorMessage, "A save with this name already exists.");
        return false;
    }

    if (!saveManager.save(targetPath, data)) {
        writeError(errorMessage, "Failed to write the edited save.");
        return false;
    }

    if (targetPath != originalPath && !saveManager.deleteSave(originalPath)) {
        saveManager.deleteSave(targetPath);
        writeError(errorMessage, "Failed to replace the original save file.");
        return false;
    }

    return true;
}