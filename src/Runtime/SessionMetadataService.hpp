#pragma once

#include <string>

#include "Runtime/SessionFormRequest.hpp"

class SaveManager;

class SessionMetadataService {
public:
    static bool prepareSessionForStart(const SessionFormRequest& request,
                                       GameSessionConfig& outSession,
                                       std::string* errorMessage);
    static bool editSavedSession(const SessionFormRequest& request,
                                 SaveManager& saveManager,
                                 const std::string& savesDirectory,
                                 std::string* errorMessage);
};