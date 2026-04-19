#pragma once

#include <string>

#include "Core/GameSessionConfig.hpp"

struct SessionFormRequest {
    std::string originalSaveName;
    GameSessionConfig session;
    std::string multiplayerPassword;
};