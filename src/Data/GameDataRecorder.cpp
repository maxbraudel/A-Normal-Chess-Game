#include "Data/GameDataRecorder.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>

#include "BuildProvenance.hpp"
#include "Config/GameConfig.hpp"
#include "Runtime/WeatherVisibility.hpp"
#include "Save/SaveManager.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/TurnPointRules.hpp"

namespace fs = std::filesystem;

namespace {

void skipWhitespace(const std::string& text, std::size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
}

std::string escapeJsonString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char current : value) {
        switch (current) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(current); break;
        }
    }
    return escaped;
}

std::string unescapeJsonString(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char current = value[i];
        if (current != '\\' || i + 1 >= value.size()) {
            result.push_back(current);
            continue;
        }

        const char escaped = value[++i];
        switch (escaped) {
            case '\\': result.push_back('\\'); break;
            case '"': result.push_back('"'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            default: result.push_back(escaped); break;
        }
    }
    return result;
}

std::size_t findValueStart(const std::string& json, const std::string& key) {
    const std::string search = "\"" + key + "\"";
    std::size_t pos = json.find(search);
    if (pos == std::string::npos) {
        return std::string::npos;
    }

    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) {
        return std::string::npos;
    }

    ++pos;
    skipWhitespace(json, pos);
    return pos;
}

std::size_t findMatchingDelimiter(const std::string& text,
                                  std::size_t start,
                                  char open,
                                  char close) {
    if (start >= text.size() || text[start] != open) {
        return std::string::npos;
    }

    bool inString = false;
    int depth = 0;
    for (std::size_t pos = start; pos < text.size(); ++pos) {
        const char current = text[pos];
        if (current == '"' && (pos == 0 || text[pos - 1] != '\\')) {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (current == open) {
            ++depth;
        } else if (current == close) {
            --depth;
            if (depth == 0) {
                return pos;
            }
        }
    }

    return std::string::npos;
}

int extractInt(const std::string& json, const std::string& key, int defaultValue) {
    std::size_t pos = findValueStart(json, key);
    if (pos == std::string::npos) {
        return defaultValue;
    }

    std::size_t end = pos;
    if (end < json.size() && (json[end] == '-' || json[end] == '+')) {
        ++end;
    }
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end])) != 0) {
        ++end;
    }
    if (end == pos) {
        return defaultValue;
    }

    try {
        return std::stoi(json.substr(pos, end - pos));
    } catch (...) {
        return defaultValue;
    }
}

long long extractLongLong(const std::string& json,
                         const std::string& key,
                         long long defaultValue) {
    std::size_t pos = findValueStart(json, key);
    if (pos == std::string::npos) {
        return defaultValue;
    }

    std::size_t end = pos;
    if (end < json.size() && (json[end] == '-' || json[end] == '+')) {
        ++end;
    }
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end])) != 0) {
        ++end;
    }
    if (end == pos) {
        return defaultValue;
    }

    try {
        return std::stoll(json.substr(pos, end - pos));
    } catch (...) {
        return defaultValue;
    }
}

std::uint64_t extractUInt64(const std::string& json,
                            const std::string& key,
                            std::uint64_t defaultValue) {
    const long long value = extractLongLong(json, key, static_cast<long long>(defaultValue));
    return (value < 0) ? defaultValue : static_cast<std::uint64_t>(value);
}

bool extractBool(const std::string& json, const std::string& key, bool defaultValue) {
    std::size_t pos = findValueStart(json, key);
    if (pos == std::string::npos) {
        return defaultValue;
    }

    if (json.compare(pos, 4, "true") == 0) {
        return true;
    }
    if (json.compare(pos, 5, "false") == 0) {
        return false;
    }

    return defaultValue;
}

std::string extractString(const std::string& json, const std::string& key) {
    std::size_t pos = findValueStart(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"') {
        return {};
    }

    ++pos;
    const std::size_t start = pos;
    while (pos < json.size()) {
        if (json[pos] == '"' && json[pos - 1] != '\\') {
            return unescapeJsonString(json.substr(start, pos - start));
        }
        ++pos;
    }

    return {};
}

std::string extractSection(const std::string& json, const std::string& key) {
    const std::size_t pos = findValueStart(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '{') {
        return {};
    }

    const std::size_t end = findMatchingDelimiter(json, pos, '{', '}');
    if (end == std::string::npos) {
        return {};
    }

    return json.substr(pos, end - pos + 1);
}

std::string extractArray(const std::string& json, const std::string& key) {
    const std::size_t pos = findValueStart(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '[') {
        return {};
    }

    const std::size_t end = findMatchingDelimiter(json, pos, '[', ']');
    if (end == std::string::npos) {
        return {};
    }

    return json.substr(pos, end - pos + 1);
}

std::vector<std::string> splitArrayElements(const std::string& arrayContent) {
    std::vector<std::string> elements;
    if (arrayContent.size() < 2 || arrayContent.front() != '[' || arrayContent.back() != ']') {
        return elements;
    }

    std::size_t pos = 1;
    while (pos + 1 < arrayContent.size()) {
        while (pos + 1 < arrayContent.size()
               && (std::isspace(static_cast<unsigned char>(arrayContent[pos])) != 0
                   || arrayContent[pos] == ',')) {
            ++pos;
        }
        if (pos + 1 >= arrayContent.size() || arrayContent[pos] == ']') {
            break;
        }

        const std::size_t start = pos;
        std::size_t end = start;
        if (arrayContent[pos] == '{') {
            end = findMatchingDelimiter(arrayContent, pos, '{', '}');
        } else if (arrayContent[pos] == '[') {
            end = findMatchingDelimiter(arrayContent, pos, '[', ']');
        } else if (arrayContent[pos] == '"') {
            ++end;
            while (end < arrayContent.size()) {
                if (arrayContent[end] == '"' && arrayContent[end - 1] != '\\') {
                    break;
                }
                ++end;
            }
        } else {
            while (end < arrayContent.size() && arrayContent[end] != ',' && arrayContent[end] != ']') {
                ++end;
            }
            if (end > start) {
                --end;
            }
        }

        if (end == std::string::npos || end < start) {
            break;
        }

        elements.push_back(arrayContent.substr(start, end - start + 1));
        pos = end + 1;
    }

    return elements;
}

std::string indentMultilineJson(const std::string& json, int indentSpaces) {
    std::istringstream input(json);
    std::ostringstream output;
    std::string line;
    const std::string indent(static_cast<std::size_t>(std::max(0, indentSpaces)), ' ');
    bool firstLine = true;
    while (std::getline(input, line)) {
        if (firstLine) {
            output << line;
            firstLine = false;
        } else {
            output << '\n' << indent << line;
        }
    }
    return output.str();
}

constexpr std::array<KingdomId, kNumKingdoms> kAllKingdoms{
    KingdomId::White,
    KingdomId::Black
};

constexpr std::array<PieceType, 6> kAllPieceTypes{
    PieceType::Pawn,
    PieceType::Knight,
    PieceType::Bishop,
    PieceType::Rook,
    PieceType::Queen,
    PieceType::King
};

constexpr std::array<BuildingType, 8> kAllBuildingTypes{
    BuildingType::Church,
    BuildingType::Mine,
    BuildingType::Farm,
    BuildingType::Barracks,
    BuildingType::WoodWall,
    BuildingType::StoneWall,
    BuildingType::Bridge,
    BuildingType::Arena
};

constexpr std::array<ChestRewardType, 3> kAllChestRewardTypes{
    ChestRewardType::Gold,
    ChestRewardType::MovementPointsMaxBonus,
    ChestRewardType::BuildPointsMaxBonus
};

constexpr std::array<TurnCommand::Type, 8> kAllTurnCommandTypes{
    TurnCommand::Move,
    TurnCommand::Build,
    TurnCommand::Produce,
    TurnCommand::Upgrade,
    TurnCommand::Marry,
    TurnCommand::FormGroup,
    TurnCommand::BreakGroup,
    TurnCommand::Disband
};

constexpr std::array<TurnCommandAuditAction, 4> kAllTurnCommandAuditActions{
    TurnCommandAuditAction::Queue,
    TurnCommandAuditAction::Replace,
    TurnCommandAuditAction::Cancel,
    TurnCommandAuditAction::Reset
};

constexpr std::array<BehavioralTelemetryOrigin, 4> kAllBehavioralTelemetryOrigins{
    BehavioralTelemetryOrigin::LocalHost,
    BehavioralTelemetryOrigin::LocalClient,
    BehavioralTelemetryOrigin::RemoteClientReported,
    BehavioralTelemetryOrigin::HostObserved
};

constexpr std::array<BehavioralTelemetryStage, 7> kAllBehavioralTelemetryStages{
    BehavioralTelemetryStage::Interaction,
    BehavioralTelemetryStage::CommandLifecycle,
    BehavioralTelemetryStage::Preview,
    BehavioralTelemetryStage::Submission,
    BehavioralTelemetryStage::Validation,
    BehavioralTelemetryStage::Commit,
    BehavioralTelemetryStage::Persistence
};

constexpr std::array<EventLog::Event::Kind, 2> kAllEventKinds{
    EventLog::Event::Kind::Message,
    EventLog::Event::Kind::Move
};

constexpr std::array<GameplayNotificationKind, 1> kAllNotificationKinds{
    GameplayNotificationKind::ChestReward
};

constexpr std::array<AutonomousUnitType, 1> kAllAutonomousUnitTypes{
    AutonomousUnitType::InfernalPiece
};

constexpr std::array<InfernalPhase, 3> kAllInfernalPhases{
    InfernalPhase::Hunting,
    InfernalPhase::Returning,
    InfernalPhase::Searching
};

constexpr std::array<WeatherDirection, kNumWeatherDirections> kAllWeatherDirections{
    WeatherDirection::North,
    WeatherDirection::South,
    WeatherDirection::East,
    WeatherDirection::West,
    WeatherDirection::NorthEast,
    WeatherDirection::NorthWest,
    WeatherDirection::SouthEast,
    WeatherDirection::SouthWest
};

constexpr std::array<XPRewardSource, kNumXPRewardSources> kAllXpRewardSources{
    XPRewardSource::KillPawn,
    XPRewardSource::KillKnight,
    XPRewardSource::KillBishop,
    XPRewardSource::KillRook,
    XPRewardSource::KillQueen,
    XPRewardSource::DestroyBlock,
    XPRewardSource::ArenaPerTurn
};

std::string serializeReferenceData();
std::string serializeSessionContext(const SaveData& snapshot);
std::string serializeConfigContext(const GameConfig& config);

const char* kingdomKeyName(KingdomId kingdom) {
    switch (kingdom) {
        case KingdomId::White: return "white";
        case KingdomId::Black: return "black";
    }

    return "unknown";
}

const char* kingdomLabelName(KingdomId kingdom) {
    switch (kingdom) {
        case KingdomId::White: return "White";
        case KingdomId::Black: return "Black";
    }

    return "Unknown";
}

const char* controllerTypeKeyName(ControllerType type) {
    switch (type) {
        case ControllerType::Human: return "human";
    }

    return "unknown";
}

const char* gameModeKeyName(GameMode mode) {
    switch (mode) {
        case GameMode::HumanVsHuman: return "human_vs_human";
    }

    return "unknown";
}

const char* pieceTypeKeyName(PieceType type) {
    switch (type) {
        case PieceType::Pawn: return "pawn";
        case PieceType::Knight: return "knight";
        case PieceType::Bishop: return "bishop";
        case PieceType::Rook: return "rook";
        case PieceType::Queen: return "queen";
        case PieceType::King: return "king";
    }

    return "unknown";
}

const char* pieceTypeLabelName(PieceType type) {
    switch (type) {
        case PieceType::Pawn: return "Pawn";
        case PieceType::Knight: return "Knight";
        case PieceType::Bishop: return "Bishop";
        case PieceType::Rook: return "Rook";
        case PieceType::Queen: return "Queen";
        case PieceType::King: return "King";
    }

    return "Unknown";
}

const char* buildingTypeKeyName(BuildingType type) {
    switch (type) {
        case BuildingType::Church: return "church";
        case BuildingType::Mine: return "mine";
        case BuildingType::Farm: return "farm";
        case BuildingType::Barracks: return "barracks";
        case BuildingType::WoodWall: return "wood_wall";
        case BuildingType::StoneWall: return "stone_wall";
        case BuildingType::Bridge: return "bridge";
        case BuildingType::Arena: return "arena";
    }

    return "unknown";
}

const char* buildingTypeLabelName(BuildingType type) {
    switch (type) {
        case BuildingType::Church: return "Church";
        case BuildingType::Mine: return "Mine";
        case BuildingType::Farm: return "Farm";
        case BuildingType::Barracks: return "Barracks";
        case BuildingType::WoodWall: return "Wood Wall";
        case BuildingType::StoneWall: return "Stone Wall";
        case BuildingType::Bridge: return "Bridge";
        case BuildingType::Arena: return "Arena";
    }

    return "Unknown";
}

const char* turnCommandTypeKeyName(TurnCommand::Type type) {
    switch (type) {
        case TurnCommand::Move: return "move";
        case TurnCommand::Build: return "build";
        case TurnCommand::Produce: return "produce";
        case TurnCommand::Upgrade: return "upgrade";
        case TurnCommand::Marry: return "marry";
        case TurnCommand::FormGroup: return "form_group";
        case TurnCommand::BreakGroup: return "break_group";
        case TurnCommand::Disband: return "disband";
    }

    return "unknown";
}

const char* turnCommandTypeLabelName(TurnCommand::Type type) {
    switch (type) {
        case TurnCommand::Move: return "Move";
        case TurnCommand::Build: return "Build";
        case TurnCommand::Produce: return "Produce";
        case TurnCommand::Upgrade: return "Upgrade";
        case TurnCommand::Marry: return "Marry";
        case TurnCommand::FormGroup: return "Form Group";
        case TurnCommand::BreakGroup: return "Break Group";
        case TurnCommand::Disband: return "Disband";
    }

    return "Unknown";
}

const char* turnCommandAuditActionKeyName(TurnCommandAuditAction action) {
    switch (action) {
        case TurnCommandAuditAction::Queue: return "queue";
        case TurnCommandAuditAction::Replace: return "replace";
        case TurnCommandAuditAction::Cancel: return "cancel";
        case TurnCommandAuditAction::Reset: return "reset";
    }

    return "unknown";
}

const char* turnCommandAuditActionLabelName(TurnCommandAuditAction action) {
    switch (action) {
        case TurnCommandAuditAction::Queue: return "Queue";
        case TurnCommandAuditAction::Replace: return "Replace";
        case TurnCommandAuditAction::Cancel: return "Cancel";
        case TurnCommandAuditAction::Reset: return "Reset";
    }

    return "Unknown";
}

const char* gameplayNotificationKindKeyName(GameplayNotificationKind kind) {
    switch (kind) {
        case GameplayNotificationKind::ChestReward: return "chest_reward";
    }

    return "unknown";
}

const char* gameplayNotificationKindLabelName(GameplayNotificationKind kind) {
    switch (kind) {
        case GameplayNotificationKind::ChestReward: return "Chest Reward";
    }

    return "Unknown";
}

const char* chestRewardTypeKeyName(ChestRewardType type) {
    switch (type) {
        case ChestRewardType::Gold: return "gold";
        case ChestRewardType::MovementPointsMaxBonus: return "movement_points_max_bonus";
        case ChestRewardType::BuildPointsMaxBonus: return "build_points_max_bonus";
    }

    return "unknown";
}

const char* chestRewardTypeLabelName(ChestRewardType type) {
    switch (type) {
        case ChestRewardType::Gold: return "Gold";
        case ChestRewardType::MovementPointsMaxBonus: return "Movement Points";
        case ChestRewardType::BuildPointsMaxBonus: return "Build Points";
    }

    return "Unknown";
}

const char* infernalPhaseKeyName(InfernalPhase phase) {
    switch (phase) {
        case InfernalPhase::Hunting: return "hunting";
        case InfernalPhase::Returning: return "returning";
        case InfernalPhase::Searching: return "searching";
    }

    return "unknown";
}

const char* eventKindKeyName(EventLog::Event::Kind kind) {
    switch (kind) {
        case EventLog::Event::Kind::Message: return "message";
        case EventLog::Event::Kind::Move: return "move";
    }

    return "unknown";
}

const char* eventKindLabelName(EventLog::Event::Kind kind) {
    switch (kind) {
        case EventLog::Event::Kind::Message: return "Message";
        case EventLog::Event::Kind::Move: return "Move";
    }

    return "Unknown";
}

const char* buildingStateKeyName(BuildingState state) {
    switch (state) {
        case BuildingState::Completed: return "completed";
        case BuildingState::UnderConstruction: return "under_construction";
    }

    return "unknown";
}

const char* mapObjectTypeKeyName(MapObjectType type) {
    switch (type) {
        case MapObjectType::Chest: return "chest";
    }

    return "unknown";
}

const char* autonomousUnitTypeKeyName(AutonomousUnitType type) {
    switch (type) {
        case AutonomousUnitType::InfernalPiece: return "infernal_piece";
    }

    return "unknown";
}

const char* weatherDirectionKeyName(WeatherDirection direction) {
    switch (direction) {
        case WeatherDirection::North: return "north";
        case WeatherDirection::South: return "south";
        case WeatherDirection::East: return "east";
        case WeatherDirection::West: return "west";
        case WeatherDirection::NorthEast: return "north_east";
        case WeatherDirection::NorthWest: return "north_west";
        case WeatherDirection::SouthEast: return "south_east";
        case WeatherDirection::SouthWest: return "south_west";
        case WeatherDirection::Count: break;
    }

    return "unknown";
}

const char* weatherDirectionLabelName(WeatherDirection direction) {
    switch (direction) {
        case WeatherDirection::North: return "North";
        case WeatherDirection::South: return "South";
        case WeatherDirection::East: return "East";
        case WeatherDirection::West: return "West";
        case WeatherDirection::NorthEast: return "North East";
        case WeatherDirection::NorthWest: return "North West";
        case WeatherDirection::SouthEast: return "South East";
        case WeatherDirection::SouthWest: return "South West";
        case WeatherDirection::Count: break;
    }

    return "Unknown";
}

const char* xpRewardSourceKeyName(XPRewardSource source) {
    switch (source) {
        case XPRewardSource::KillPawn: return "kill_pawn";
        case XPRewardSource::KillKnight: return "kill_knight";
        case XPRewardSource::KillBishop: return "kill_bishop";
        case XPRewardSource::KillRook: return "kill_rook";
        case XPRewardSource::KillQueen: return "kill_queen";
        case XPRewardSource::DestroyBlock: return "destroy_block";
        case XPRewardSource::ArenaPerTurn: return "arena_per_turn";
        case XPRewardSource::Count: break;
    }

    return "unknown";
}

const char* xpRewardSourceLabelName(XPRewardSource source) {
    switch (source) {
        case XPRewardSource::KillPawn: return "Kill Pawn";
        case XPRewardSource::KillKnight: return "Kill Knight";
        case XPRewardSource::KillBishop: return "Kill Bishop";
        case XPRewardSource::KillRook: return "Kill Rook";
        case XPRewardSource::KillQueen: return "Kill Queen";
        case XPRewardSource::DestroyBlock: return "Destroy Block";
        case XPRewardSource::ArenaPerTurn: return "Arena Per Turn";
        case XPRewardSource::Count: break;
    }

    return "Unknown";
}

std::string serializeCellPosition(sf::Vector2i cell) {
    std::ostringstream output;
    output << "{"
           << "\"x\":" << cell.x << ","
           << "\"y\":" << cell.y
           << "}";
    return output.str();
}

std::string serializeChestReward(const ChestReward& reward) {
    std::ostringstream output;
    output << "{"
           << "\"typeId\":" << static_cast<int>(reward.type) << ","
        << "\"typeKey\":\"" << chestRewardTypeKeyName(reward.type) << "\","
        << "\"typeLabel\":\"" << chestRewardTypeLabelName(reward.type) << "\",";
    output << "\"amount\":" << reward.amount << ","
           << "\"description\":\"" << escapeJsonString(describeChestReward(reward)) << "\""
           << "}";
    return output.str();
}

std::string serializeResourceIncomeProfile(const ResourceIncomeProfile& profile) {
    std::ostringstream output;
    output << "{"
           << "\"firstCellIncomePerTurn\":" << profile.firstCellIncomePerTurn << ","
           << "\"additionalCellDecrement\":" << profile.additionalCellDecrement << ","
           << "\"minimumCellIncomePerTurn\":" << profile.minimumCellIncomePerTurn
           << "}";
    return output.str();
}

std::string serializeXPRewardProfile(const XPRewardProfile& profile) {
    std::ostringstream output;
    output << "{"
           << "\"mean\":" << profile.mean << ","
           << "\"sigmaMultiplierTimes100\":" << profile.sigmaMultiplierTimes100 << ","
           << "\"clampSigmaMultiplierTimes100\":" << profile.clampSigmaMultiplierTimes100 << ","
           << "\"minimum\":" << profile.minimum
           << "}";
    return output.str();
}

std::string serializeWeatherFront(const WeatherFrontDescriptor& front) {
    std::ostringstream output;
    output << "{"
           << "\"directionId\":" << static_cast<int>(front.direction) << ","
           << "\"directionKey\":\"" << weatherDirectionKeyName(front.direction) << "\","
           << "\"directionLabel\":\"" << weatherDirectionLabelName(front.direction) << "\","
           << "\"currentTurnStep\":" << front.currentTurnStep << ","
           << "\"totalTurnSteps\":" << front.totalTurnSteps << ","
           << "\"centerStartXTimes1000\":" << front.centerStartXTimes1000 << ","
           << "\"centerStartYTimes1000\":" << front.centerStartYTimes1000 << ","
           << "\"stepXTimes1000\":" << front.stepXTimes1000 << ","
           << "\"stepYTimes1000\":" << front.stepYTimes1000 << ","
           << "\"radiusAlongTimes1000\":" << front.radiusAlongTimes1000 << ","
           << "\"radiusAcrossTimes1000\":" << front.radiusAcrossTimes1000 << ","
           << "\"shapeSeed\":" << front.shapeSeed << ","
           << "\"densitySeed\":" << front.densitySeed
           << "}";
    return output.str();
}

const Piece* findPieceAtPosition(const SaveData& snapshot, sf::Vector2i cell) {
    for (KingdomId kingdom : kAllKingdoms) {
        for (const Piece& piece : snapshot.kingdoms[kingdomIndex(kingdom)].pieces) {
            if (piece.position == cell) {
                return &piece;
            }
        }
    }

    return nullptr;
}

const MapObject* findMapObjectById(const std::vector<MapObject>& objects, int objectId) {
    if (objectId < 0) {
        return nullptr;
    }

    for (const MapObject& object : objects) {
        if (object.id == objectId) {
            return &object;
        }
    }

    return nullptr;
}

const AutonomousUnit* findAutonomousUnitById(const std::vector<AutonomousUnit>& units, int unitId) {
    if (unitId < 0) {
        return nullptr;
    }

    for (const AutonomousUnit& unit : units) {
        if (unit.id == unitId) {
            return &unit;
        }
    }

    return nullptr;
}

template <typename Callback>
void forEachPieceInSnapshot(const SaveData& snapshot, Callback&& callback) {
    for (KingdomId kingdom : kAllKingdoms) {
        for (const Piece& piece : snapshot.kingdoms[kingdomIndex(kingdom)].pieces) {
            callback(piece);
        }
    }
}

template <typename Callback>
void forEachBuildingInSnapshot(const SaveData& snapshot, Callback&& callback) {
    for (KingdomId kingdom : kAllKingdoms) {
        for (const Building& building : snapshot.kingdoms[kingdomIndex(kingdom)].buildings) {
            callback(building);
        }
    }
    for (const Building& building : snapshot.publicBuildings) {
        callback(building);
    }
}

const Piece* findPieceById(const SaveData& snapshot, int pieceId) {
    if (pieceId < 0) {
        return nullptr;
    }

    for (KingdomId kingdom : kAllKingdoms) {
        for (const Piece& piece : snapshot.kingdoms[kingdomIndex(kingdom)].pieces) {
            if (piece.id == pieceId) {
                return &piece;
            }
        }
    }

    return nullptr;
}

const Building* findBuildingById(const SaveData& snapshot, int buildingId) {
    if (buildingId < 0) {
        return nullptr;
    }

    for (KingdomId kingdom : kAllKingdoms) {
        for (const Building& building : snapshot.kingdoms[kingdomIndex(kingdom)].buildings) {
            if (building.id == buildingId) {
                return &building;
            }
        }
    }

    for (const Building& building : snapshot.publicBuildings) {
        if (building.id == buildingId) {
            return &building;
        }
    }

    return nullptr;
}

std::string serializeJsonArray(const std::vector<std::string>& items) {
    std::ostringstream output;
    output << "[";
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        output << items[index];
    }
    output << "]";
    return output.str();
}

std::string hashHex(const std::string& value) {
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned char current : value) {
        hash ^= static_cast<std::uint64_t>(current);
        hash *= 1099511628211ull;
    }

    std::ostringstream output;
    output << std::hex << std::setw(16) << std::setfill('0') << hash;
    return output.str();
}

std::string weatherFrontIdentity(const WeatherFrontDescriptor& front) {
    std::ostringstream output;
    output << static_cast<int>(front.direction)
           << ':' << front.shapeSeed
           << ':' << front.densitySeed
           << ':' << front.centerStartXTimes1000
           << ':' << front.centerStartYTimes1000;
    return output.str();
}

bool snapshotHasWeatherFrontIdentity(const SaveData& snapshot, const std::string& identity) {
    for (const WeatherFrontDescriptor& front : snapshot.weatherSystemState.activeFronts) {
        if (weatherFrontIdentity(front) == identity) {
            return true;
        }
    }

    if (snapshot.weatherSystemState.activeFronts.empty() && snapshot.weatherSystemState.hasActiveFront) {
        return weatherFrontIdentity(snapshot.weatherSystemState.activeFront) == identity;
    }

    return false;
}

std::string pieceRemovalCause(const GameDataTurnRecord& record,
                              const Piece& previousPiece) {
    if (previousPiece.kingdom == record.committedActiveKingdom) {
        for (const TurnCommand& command : record.queuedCommands) {
            if (command.type == TurnCommand::Disband && command.pieceId == previousPiece.id) {
                return "disbanded";
            }
        }
    }

    for (const TurnCommand& command : record.queuedCommands) {
        if (command.type == TurnCommand::Move && command.destination == previousPiece.position) {
            return "captured";
        }
    }

    return "unknown";
}

std::string serializeProvenance(const SaveData& snapshot,
                                const GameConfig& config) {
    const std::string referenceData = serializeReferenceData();
    const std::string sessionContext = serializeSessionContext(snapshot);
    const std::string configContext = serializeConfigContext(config);

    std::ostringstream output;
    output << "{";
    output << "\"generator\":\"GameDataRecorder\",";
    output << "\"generatorSchemaVersion\":" << GameDataRecorder::kSchemaVersion << ",";
    output << "\"formatFamily\":\"A Normal Chess Game Data Companion\",";
    output << "\"build\":{";
    output << "\"configuredAtUtc\":\"" << escapeJsonString(BuildProvenance::kConfiguredAtUtc) << "\",";
    output << "\"buildType\":\"" << escapeJsonString(BuildProvenance::kBuildType) << "\",";
    output << "\"cmakeGenerator\":\"" << escapeJsonString(BuildProvenance::kCMakeGenerator) << "\",";
    output << "\"compilerId\":\"" << escapeJsonString(BuildProvenance::kCompilerId) << "\",";
    output << "\"compilerVersion\":\"" << escapeJsonString(BuildProvenance::kCompilerVersion) << "\",";
    output << "\"systemName\":\"" << escapeJsonString(BuildProvenance::kSystemName) << "\"";
    output << "},";
    output << "\"git\":{";
    output << "\"commit\":\"" << escapeJsonString(BuildProvenance::kGitCommit) << "\",";
    output << "\"branch\":\"" << escapeJsonString(BuildProvenance::kGitBranch) << "\",";
    output << "\"dirty\":" << (BuildProvenance::kGitDirty ? "true" : "false");
    output << "},";
    output << "\"referenceDataHash\":\"" << hashHex(referenceData) << "\",";
    output << "\"sessionContextHash\":\"" << hashHex(sessionContext) << "\",";
    output << "\"configContextHash\":\"" << hashHex(configContext) << "\"";
    output << "}";
    return output.str();
}

std::optional<ResourceIncomeProfile> incomeProfileForBuildingType(BuildingType type,
                                                                  const GameConfig& config) {
    switch (type) {
        case BuildingType::Mine:
            return config.getMineIncomeProfile();
        case BuildingType::Farm:
            return config.getFarmIncomeProfile();
        default:
            return std::nullopt;
    }
}

int calculatePieceUpkeepCost(const std::vector<Piece>& pieces, const GameConfig& config) {
    int total = 0;
    for (const Piece& piece : pieces) {
        total += config.getPieceUpkeepCost(piece.type);
    }
    return total;
}

ResourceIncomeBreakdown calculateSnapshotResourceIncomeBreakdown(const SaveData& snapshot,
                                                                 const Building& building,
                                                                 const GameConfig& config) {
    const std::optional<ResourceIncomeProfile> incomeProfile =
        incomeProfileForBuildingType(building.type, config);
    if (!incomeProfile.has_value() || !building.hasActiveGameplayEffects()) {
        return {};
    }

    int whiteOccupiedCells = 0;
    int blackOccupiedCells = 0;
    for (const sf::Vector2i& occupiedCell : building.getOccupiedCells()) {
        const Piece* piece = findPieceAtPosition(snapshot, occupiedCell);
        if (piece == nullptr) {
            continue;
        }

        if (piece->kingdom == KingdomId::White) {
            ++whiteOccupiedCells;
        } else {
            ++blackOccupiedCells;
        }
    }

    return EconomySystem::calculateResourceIncomeFromOccupation(
        whiteOccupiedCells,
        blackOccupiedCells,
        incomeProfile.value());
}

TurnEconomyBreakdown calculateSnapshotTurnEconomy(const SaveData& snapshot,
                                                  KingdomId kingdom,
                                                  const GameConfig& config) {
    TurnEconomyBreakdown breakdown;
    const SaveData::KingdomData& kingdomData = snapshot.kingdoms[kingdomIndex(kingdom)];
    breakdown.currentGold = kingdomData.gold;
    for (const Building& building : snapshot.publicBuildings) {
        const ResourceIncomeBreakdown incomeBreakdown =
            calculateSnapshotResourceIncomeBreakdown(snapshot, building, config);
        breakdown.grossIncome += incomeBreakdown.incomeFor(kingdom);
    }
    breakdown.upkeepCost = calculatePieceUpkeepCost(kingdomData.pieces, config);
    breakdown.netIncome = breakdown.grossIncome - breakdown.upkeepCost;
    breakdown.endingGold = breakdown.currentGold + breakdown.netIncome;
    return breakdown;
}

std::string serializePieceTypeCounts(const std::vector<Piece>& pieces) {
    std::array<int, kAllPieceTypes.size()> counts{};
    for (const Piece& piece : pieces) {
        counts[static_cast<std::size_t>(piece.type)] += 1;
    }

    std::ostringstream output;
    output << "[";
    for (std::size_t index = 0; index < kAllPieceTypes.size(); ++index) {
        if (index > 0) {
            output << ",";
        }

        const PieceType type = kAllPieceTypes[index];
        output << "{"
               << "\"pieceTypeId\":" << static_cast<int>(type) << ","
               << "\"pieceTypeKey\":\"" << pieceTypeKeyName(type) << "\","
               << "\"pieceTypeLabel\":\"" << pieceTypeLabelName(type) << "\","
               << "\"count\":" << counts[index]
               << "}";
    }
    output << "]";
    return output.str();
}

std::string serializeBuildingTypeCounts(const std::vector<Building>& buildings) {
    std::array<int, kAllBuildingTypes.size()> counts{};
    for (const Building& building : buildings) {
        counts[static_cast<std::size_t>(building.type)] += 1;
    }

    std::ostringstream output;
    output << "[";
    for (std::size_t index = 0; index < kAllBuildingTypes.size(); ++index) {
        if (index > 0) {
            output << ",";
        }

        const BuildingType type = kAllBuildingTypes[index];
        output << "{"
               << "\"buildingTypeId\":" << static_cast<int>(type) << ","
               << "\"buildingTypeKey\":\"" << buildingTypeKeyName(type) << "\","
               << "\"buildingTypeLabel\":\"" << buildingTypeLabelName(type) << "\","
               << "\"count\":" << counts[index]
               << "}";
    }
    output << "]";
    return output.str();
}

std::string serializeReferenceData() {
    std::ostringstream output;
    output << "{";
    output << "\"kingdoms\":[";
    for (std::size_t index = 0; index < kAllKingdoms.size(); ++index) {
        if (index > 0) {
            output << ",";
        }

        const KingdomId kingdom = kAllKingdoms[index];
        output << "{"
               << "\"id\":" << static_cast<int>(kingdom) << ","
               << "\"key\":\"" << kingdomKeyName(kingdom) << "\","
               << "\"label\":\"" << kingdomLabelName(kingdom) << "\""
               << "}";
    }
    output << "],";

    output << "\"controllers\":[{"
           << "\"id\":" << static_cast<int>(ControllerType::Human) << ","
           << "\"key\":\"" << controllerTypeKeyName(ControllerType::Human) << "\","
           << "\"label\":\"" << controllerTypeLabel(ControllerType::Human) << "\""
           << "}],";

    output << "\"gameModes\":[{"
           << "\"id\":" << static_cast<int>(GameMode::HumanVsHuman) << ","
           << "\"key\":\"" << gameModeKeyName(GameMode::HumanVsHuman) << "\","
           << "\"label\":\"" << gameModeLabel(GameMode::HumanVsHuman) << "\""
           << "}],";

    output << "\"pieceTypes\":[";
    for (std::size_t index = 0; index < kAllPieceTypes.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const PieceType type = kAllPieceTypes[index];
        output << "{"
               << "\"id\":" << static_cast<int>(type) << ","
               << "\"key\":\"" << pieceTypeKeyName(type) << "\","
               << "\"label\":\"" << pieceTypeLabelName(type) << "\""
               << "}";
    }
    output << "],";

    output << "\"buildingTypes\":[";
    for (std::size_t index = 0; index < kAllBuildingTypes.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const BuildingType type = kAllBuildingTypes[index];
        output << "{"
               << "\"id\":" << static_cast<int>(type) << ","
               << "\"key\":\"" << buildingTypeKeyName(type) << "\","
               << "\"label\":\"" << buildingTypeLabelName(type) << "\""
               << "}";
    }
    output << "],";

    output << "\"chestRewardTypes\":[";
    for (std::size_t index = 0; index < kAllChestRewardTypes.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const ChestRewardType type = kAllChestRewardTypes[index];
        output << "{"
               << "\"id\":" << static_cast<int>(type) << ","
               << "\"key\":\"" << chestRewardTypeKeyName(type) << "\","
               << "\"label\":\"" << chestRewardTypeLabelName(type) << "\""
               << "}";
    }
    output << "],";

    output << "\"turnCommandTypes\":[";
    for (std::size_t index = 0; index < kAllTurnCommandTypes.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const TurnCommand::Type type = kAllTurnCommandTypes[index];
        output << "{"
               << "\"id\":" << static_cast<int>(type) << ","
               << "\"key\":\"" << turnCommandTypeKeyName(type) << "\","
               << "\"label\":\"" << turnCommandTypeLabelName(type) << "\""
               << "}";
    }
    output << "],";

    output << "\"turnCommandAuditActions\":[";
    for (std::size_t index = 0; index < kAllTurnCommandAuditActions.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const TurnCommandAuditAction action = kAllTurnCommandAuditActions[index];
        output << "{"
               << "\"id\":" << static_cast<int>(action) << ","
               << "\"key\":\"" << turnCommandAuditActionKeyName(action) << "\","
               << "\"label\":\"" << turnCommandAuditActionLabelName(action) << "\""
               << "}";
    }
    output << "],";

    output << "\"behavioralTelemetryOrigins\":[";
    for (std::size_t index = 0; index < kAllBehavioralTelemetryOrigins.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const BehavioralTelemetryOrigin origin = kAllBehavioralTelemetryOrigins[index];
        output << "{"
               << "\"id\":" << static_cast<int>(origin) << ","
               << "\"key\":\"" << behavioralTelemetryOriginKey(origin) << "\","
               << "\"label\":\"" << behavioralTelemetryOriginLabel(origin) << "\""
               << "}";
    }
    output << "],";

    output << "\"behavioralTelemetryStages\":[";
    for (std::size_t index = 0; index < kAllBehavioralTelemetryStages.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const BehavioralTelemetryStage stage = kAllBehavioralTelemetryStages[index];
        output << "{"
               << "\"id\":" << static_cast<int>(stage) << ","
               << "\"key\":\"" << behavioralTelemetryStageKey(stage) << "\","
               << "\"label\":\"" << behavioralTelemetryStageLabel(stage) << "\""
               << "}";
    }
    output << "],";

    output << "\"eventKinds\":[";
    for (std::size_t index = 0; index < kAllEventKinds.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const EventLog::Event::Kind kind = kAllEventKinds[index];
        output << "{"
               << "\"id\":" << static_cast<int>(kind) << ","
               << "\"key\":\"" << eventKindKeyName(kind) << "\","
               << "\"label\":\"" << eventKindLabelName(kind) << "\""
               << "}";
    }
    output << "],";

    output << "\"gameplayNotificationKinds\":[";
    for (std::size_t index = 0; index < kAllNotificationKinds.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const GameplayNotificationKind kind = kAllNotificationKinds[index];
        output << "{"
               << "\"id\":" << static_cast<int>(kind) << ","
               << "\"key\":\"" << gameplayNotificationKindKeyName(kind) << "\","
               << "\"label\":\"" << gameplayNotificationKindLabelName(kind) << "\""
               << "}";
    }
    output << "],";

    output << "\"autonomousUnitTypes\":[";
    for (std::size_t index = 0; index < kAllAutonomousUnitTypes.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const AutonomousUnitType type = kAllAutonomousUnitTypes[index];
        output << "{"
               << "\"id\":" << static_cast<int>(type) << ","
               << "\"key\":\"infernal_piece\","
               << "\"label\":\"" << autonomousUnitTypeDisplayName(type) << "\""
               << "}";
    }
    output << "],";

    output << "\"infernalPhases\":[";
    for (std::size_t index = 0; index < kAllInfernalPhases.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const InfernalPhase phase = kAllInfernalPhases[index];
        output << "{"
               << "\"id\":" << static_cast<int>(phase) << ","
               << "\"key\":\"" << infernalPhaseKeyName(phase) << "\","
               << "\"label\":\"" << escapeJsonString(infernalPhaseDisplayName(phase)) << "\""
               << "}";
    }
    output << "],";

    output << "\"weatherDirections\":[";
    for (std::size_t index = 0; index < kAllWeatherDirections.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const WeatherDirection direction = kAllWeatherDirections[index];
        output << "{"
               << "\"id\":" << static_cast<int>(direction) << ","
               << "\"key\":\"" << weatherDirectionKeyName(direction) << "\","
               << "\"label\":\"" << weatherDirectionLabelName(direction) << "\""
               << "}";
    }
    output << "],";

    output << "\"xpRewardSources\":[";
    for (std::size_t index = 0; index < kAllXpRewardSources.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const XPRewardSource source = kAllXpRewardSources[index];
        output << "{"
               << "\"id\":" << static_cast<int>(source) << ","
               << "\"key\":\"" << xpRewardSourceKeyName(source) << "\","
               << "\"label\":\"" << xpRewardSourceLabelName(source) << "\""
               << "}";
    }
    output << "]";
    output << "}";
    return output.str();
}

std::string serializeSessionContext(const SaveData& snapshot) {
    const GameMode mode = gameModeFromParticipants(snapshot.sessionKingdoms);

    std::ostringstream output;
    output << "{";
    output << "\"saveName\":\"" << escapeJsonString(snapshot.gameName) << "\",";
    output << "\"worldSeed\":" << snapshot.worldSeed << ",";
    output << "\"mapRadius\":" << snapshot.mapRadius << ",";
    output << "\"turnNumber\":" << snapshot.turnNumber << ",";
    output << "\"activeKingdomId\":" << static_cast<int>(snapshot.activeKingdom) << ",";
    output << "\"activeKingdomKey\":\"" << kingdomKeyName(snapshot.activeKingdom) << "\",";
    output << "\"activeKingdomLabel\":\"" << kingdomLabelName(snapshot.activeKingdom) << "\",";
    output << "\"gameModeId\":" << static_cast<int>(mode) << ",";
    output << "\"gameModeKey\":\"" << gameModeKeyName(mode) << "\",";
    output << "\"gameModeLabel\":\"" << gameModeLabel(mode) << "\",";
    output << "\"participants\":[";
    for (std::size_t index = 0; index < snapshot.sessionKingdoms.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const KingdomParticipantConfig& participant = snapshot.sessionKingdoms[index];
        output << "{"
               << "\"kingdomId\":" << static_cast<int>(participant.kingdom) << ","
               << "\"kingdomKey\":\"" << kingdomKeyName(participant.kingdom) << "\","
               << "\"controllerId\":" << static_cast<int>(participant.controller) << ","
               << "\"controllerKey\":\"" << controllerTypeKeyName(participant.controller) << "\","
               << "\"controllerLabel\":\"" << controllerTypeLabel(participant.controller) << "\","
               << "\"participantName\":\"" << escapeJsonString(participant.participantName) << "\""
               << "}";
    }
    output << "],";
    output << "\"multiplayer\":{";
    output << "\"enabled\":" << (snapshot.multiplayer.enabled ? "true" : "false") << ",";
    output << "\"port\":" << snapshot.multiplayer.port << ",";
    output << "\"hasPassword\":" << (!snapshot.multiplayer.passwordHash.empty() ? "true" : "false") << ",";
    output << "\"protocolVersion\":" << snapshot.multiplayer.protocolVersion;
    output << "},";
    output << "\"options\":{";
    output << "\"tacticalGridEnabled\":" << (snapshot.tacticalGridEnabled ? "true" : "false") << ",";
    output << "\"sharedTurnPreviewEnabled\":" << (snapshot.sharedTurnPreviewEnabled ? "true" : "false") << ",";
        output << "\"dataCollectionEnabled\":" << (snapshot.dataCollectionEnabled ? "true" : "false") << ",";
        output << "\"behavioralTelemetryEnabled\":"
            << (snapshot.behavioralTelemetryEnabled ? "true" : "false");
    output << "}";
    output << "}";
    return output.str();
}

std::string serializeConfigContext(const GameConfig& config) {
    std::ostringstream output;
    output << "{";

    output << "\"map\":{";
    output << "\"mapRadius\":" << config.getMapRadius() << ",";
    output << "\"numMines\":" << config.getNumMines() << ",";
    output << "\"numFarms\":" << config.getNumFarms() << ",";
    output << "\"minPublicBuildingDistance\":" << config.getMinPublicBuildingDistance() << ",";
    output << "\"playerSpawnZonePercent\":" << config.getPlayerSpawnZonePercent() << ",";
    output << "\"aiSpawnZonePercent\":" << config.getAISpawnZonePercent();
    output << "},";

    output << "\"economy\":{";
    output << "\"startingGold\":" << config.getStartingGold() << ",";
    output << "\"movementPointsPerTurn\":" << config.getMovementPointsPerTurn() << ",";
    output << "\"buildPointsPerTurn\":" << config.getBuildPointsPerTurn() << ",";
    output << "\"mineIncomeProfile\":" << serializeResourceIncomeProfile(config.getMineIncomeProfile()) << ",";
    output << "\"farmIncomeProfile\":" << serializeResourceIncomeProfile(config.getFarmIncomeProfile()) << ",";
    output << "\"pieceRules\":[";
    for (std::size_t index = 0; index < kAllPieceTypes.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const PieceType type = kAllPieceTypes[index];
        output << "{"
               << "\"pieceTypeId\":" << static_cast<int>(type) << ","
               << "\"pieceTypeKey\":\"" << pieceTypeKeyName(type) << "\","
               << "\"pieceTypeLabel\":\"" << pieceTypeLabelName(type) << "\","
               << "\"movePointCost\":" << config.getMovePointCost(type) << ","
               << "\"moveAllowancePerTurn\":" << config.getMoveAllowancePerTurn(type) << ","
               << "\"upkeepCost\":" << config.getPieceUpkeepCost(type) << ","
               << "\"recruitCost\":" << config.getRecruitCost(type) << ","
               << "\"productionTurns\":" << config.getProductionTurns(type)
               << "}";
    }
    output << "],";
    output << "\"upgradeRules\":[";
    output << "{\"fromPieceTypeId\":0,\"fromPieceTypeKey\":\"pawn\",\"toPieceTypeId\":1,\"toPieceTypeKey\":\"knight\",\"cost\":" << config.getUpgradeCost(PieceType::Pawn, PieceType::Knight) << "},";
    output << "{\"fromPieceTypeId\":0,\"fromPieceTypeKey\":\"pawn\",\"toPieceTypeId\":2,\"toPieceTypeKey\":\"bishop\",\"cost\":" << config.getUpgradeCost(PieceType::Pawn, PieceType::Bishop) << "},";
    output << "{\"fromPieceTypeId\":1,\"fromPieceTypeKey\":\"knight\",\"toPieceTypeId\":3,\"toPieceTypeKey\":\"rook\",\"cost\":" << config.getUpgradeCost(PieceType::Knight, PieceType::Rook) << "},";
    output << "{\"fromPieceTypeId\":2,\"fromPieceTypeKey\":\"bishop\",\"toPieceTypeId\":3,\"toPieceTypeKey\":\"rook\",\"cost\":" << config.getUpgradeCost(PieceType::Bishop, PieceType::Rook) << "}";
    output << "],";
    output << "\"buildingRules\":[";
    for (std::size_t index = 0; index < kAllBuildingTypes.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const BuildingType type = kAllBuildingTypes[index];
        output << "{"
               << "\"buildingTypeId\":" << static_cast<int>(type) << ","
               << "\"buildingTypeKey\":\"" << buildingTypeKeyName(type) << "\","
               << "\"buildingTypeLabel\":\"" << buildingTypeLabelName(type) << "\","
               << "\"buildPointCost\":" << config.getBuildPointCost(type) << ","
               << "\"repairCostPerCell\":" << config.getRepairCostPerCell(type) << ","
               << "\"destroyedCellsRequired\":" << config.getDestroyedCellsRequired(type) << ","
               << "\"width\":" << config.getBuildingWidth(type) << ","
               << "\"height\":" << config.getBuildingHeight(type)
               << "}";
    }
    output << "]";
    output << "},";

    output << "\"combat\":{";
    output << "\"woodWallHP\":" << config.getWoodWallHP() << ",";
    output << "\"stoneWallHP\":" << config.getStoneWallHP() << ",";
    output << "\"barracksCellHP\":" << config.getBarracksCellHP() << ",";
    output << "\"globalMaxRange\":" << config.getGlobalMaxRange();
    output << "},";

    output << "\"xp\":{";
    output << "\"thresholdPawnToKnightOrBishop\":" << config.getXPThresholdPawnToKnightOrBishop() << ",";
    output << "\"thresholdToRook\":" << config.getXPThresholdToRook() << ",";
    output << "\"rewardSources\":[";
    for (std::size_t index = 0; index < kAllXpRewardSources.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const XPRewardSource source = kAllXpRewardSources[index];
        output << "{"
               << "\"sourceId\":" << static_cast<int>(source) << ","
               << "\"sourceKey\":\"" << xpRewardSourceKeyName(source) << "\","
               << "\"sourceLabel\":\"" << xpRewardSourceLabelName(source) << "\","
               << "\"profile\":" << serializeXPRewardProfile(config.getXPRewardProfile(source))
               << "}";
    }
    output << "]";
    output << "},";

    output << "\"chest\":{";
    output << "\"minSpawnTurn\":" << config.getChestMinSpawnTurn() << ",";
    output << "\"respawnCooldownTurns\":" << config.getChestRespawnCooldownTurns() << ",";
    output << "\"spawnRetryTurns\":" << config.getChestSpawnRetryTurns() << ",";
    output << "\"weibullShapeTimes100\":" << config.getChestWeibullShapeTimes100() << ",";
    output << "\"weibullScaleTurns\":" << config.getChestWeibullScaleTurns() << ",";
    output << "\"minDistanceFromKings\":" << config.getChestMinDistanceFromKings() << ",";
    output << "\"goldRewardProfile\":" << serializeXPRewardProfile(config.getChestGoldRewardProfile()) << ",";
    output << "\"goldRewardAmount\":" << config.getChestGoldRewardAmount() << ",";
    output << "\"movementBonusAmount\":" << config.getChestMovementBonusAmount() << ",";
    output << "\"buildBonusAmount\":" << config.getChestBuildBonusAmount() << ",";
    output << "\"lateGameTurn\":" << config.getChestLateGameTurn() << ",";
    output << "\"earlyWeights\":{";
    output << "\"gold\":" << config.getChestEarlyGoldWeight() << ",";
    output << "\"movementBonus\":" << config.getChestEarlyMovementBonusWeight() << ",";
    output << "\"buildBonus\":" << config.getChestEarlyBuildBonusWeight();
    output << "},";
    output << "\"lateWeights\":{";
    output << "\"gold\":" << config.getChestLateGoldWeight() << ",";
    output << "\"movementBonus\":" << config.getChestLateMovementBonusWeight() << ",";
    output << "\"buildBonus\":" << config.getChestLateBuildBonusWeight();
    output << "},";
    output << "\"currentLootCatchUpEnabled\":"
           << (config.isChestCurrentLootCatchUpEnabled() ? "true" : "false");
    output << "},";

    output << "\"infernal\":{";
    output << "\"minSpawnTurn\":" << config.getInfernalMinSpawnTurn() << ",";
    output << "\"respawnCooldownTurns\":" << config.getInfernalRespawnCooldownTurns() << ",";
    output << "\"spawnRetryTurns\":" << config.getInfernalSpawnRetryTurns() << ",";
    output << "\"poissonLambdaBaseTimes1000\":" << config.getInfernalPoissonLambdaBaseTimes1000() << ",";
    output << "\"poissonLambdaPerDebtTimes1000\":" << config.getInfernalPoissonLambdaPerDebtTimes1000() << ",";
    output << "\"poissonLambdaCapTimes1000\":" << config.getInfernalPoissonLambdaCapTimes1000() << ",";
    output << "\"bloodDebtDecayPercent\":" << config.getInfernalBloodDebtDecayPercent() << ",";
    output << "\"bloodDebtForStructureDamage\":" << config.getInfernalBloodDebtForStructureDamage() << ",";
    output << "\"searchingRandomMoveChanceTimes1000\":" << config.getInfernalSearchingRandomMoveChanceTimes1000() << ",";
    output << "\"targetWeights\":[";
    for (std::size_t index = 0; index < kAllPieceTypes.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const PieceType type = kAllPieceTypes[index];
        output << "{"
               << "\"pieceTypeId\":" << static_cast<int>(type) << ","
               << "\"pieceTypeKey\":\"" << pieceTypeKeyName(type) << "\","
               << "\"weight\":" << config.getInfernalTargetWeight(type) << ","
               << "\"bloodDebtForCapturedPiece\":" << config.getInfernalBloodDebtForCapturedPiece(type)
               << "}";
    }
    output << "]";
    output << "},";

    output << "\"weather\":{";
    output << "\"cooldownMinTurns\":" << config.getWeatherCooldownMinTurns() << ",";
    output << "\"spawnBlockedWhileFrontActive\":"
           << (config.isWeatherSpawnBlockedWhileFrontActive() ? "true" : "false") << ",";
    output << "\"arrivalGammaShapeTimes100\":" << config.getWeatherArrivalGammaShapeTimes100() << ",";
    output << "\"arrivalGammaScaleTimes100\":" << config.getWeatherArrivalGammaScaleTimes100() << ",";
    output << "\"durationGammaShapeTimes100\":" << config.getWeatherDurationGammaShapeTimes100() << ",";
    output << "\"durationGammaScaleTimes100\":" << config.getWeatherDurationGammaScaleTimes100() << ",";
    output << "\"speedBlocksPer100Turns\":" << config.getWeatherSpeedBlocksPer100Turns() << ",";
    output << "\"directionWeights\":[";
    const std::array<int, kNumWeatherDirections> directionWeights = config.getWeatherDirectionWeights();
    for (std::size_t index = 0; index < kAllWeatherDirections.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const WeatherDirection direction = kAllWeatherDirections[index];
        output << "{"
               << "\"directionId\":" << static_cast<int>(direction) << ","
               << "\"directionKey\":\"" << weatherDirectionKeyName(direction) << "\","
               << "\"weight\":" << directionWeights[index]
               << "}";
    }
    output << "],";
    output << "\"entryCenterWeightTimes100\":" << config.getWeatherEntryCenterWeightTimes100() << ",";
    output << "\"entryCornerWeightTimes100\":" << config.getWeatherEntryCornerWeightTimes100() << ",";
    output << "\"coverageMinPercent\":" << config.getWeatherCoverageMinPercent() << ",";
    output << "\"coverageMaxPercent\":" << config.getWeatherCoverageMaxPercent() << ",";
    output << "\"aspectRatioMinTimes100\":" << config.getWeatherAspectRatioMinTimes100() << ",";
    output << "\"aspectRatioMaxTimes100\":" << config.getWeatherAspectRatioMaxTimes100() << ",";
    output << "\"shapeNoiseCellSpan\":" << config.getWeatherShapeNoiseCellSpan() << ",";
    output << "\"shapeNoiseAmplitudePercent\":" << config.getWeatherShapeNoiseAmplitudePercent() << ",";
    output << "\"edgeSoftnessPercent\":" << config.getWeatherEdgeSoftnessPercent() << ",";
    output << "\"alphaBasePercent\":" << config.getWeatherAlphaBasePercent() << ",";
    output << "\"alphaMinPercent\":" << config.getWeatherAlphaMinPercent() << ",";
    output << "\"alphaMaxPercent\":" << config.getWeatherAlphaMaxPercent() << ",";
    output << "\"densityMuTimes100\":" << config.getWeatherDensityMuTimes100() << ",";
    output << "\"densitySigmaTimes100\":" << config.getWeatherDensitySigmaTimes100();
    output << "}";

    output << "}";
    return output.str();
}

std::string serializeValidation(const CheckTurnValidation& validation) {
    std::ostringstream output;
    output << "{"
           << "\"valid\":" << (validation.valid ? "true" : "false") << ","
           << "\"activeKingInCheck\":" << (validation.activeKingInCheck ? "true" : "false") << ","
           << "\"projectedKingInCheck\":" << (validation.projectedKingInCheck ? "true" : "false") << ","
           << "\"hasAnyLegalResponse\":" << (validation.hasAnyLegalResponse ? "true" : "false") << ","
           << "\"requiresSingleResponseMove\":" << (validation.requiresSingleResponseMove ? "true" : "false") << ","
           << "\"hasQueuedMove\":" << (validation.hasQueuedMove ? "true" : "false") << ","
           << "\"bankrupt\":" << (validation.bankrupt ? "true" : "false") << ","
           << "\"projectedEndingGold\":" << validation.projectedEndingGold << ","
           << "\"errorMessage\":\"" << escapeJsonString(validation.errorMessage) << "\""
           << "}";
    return output.str();
}

CheckTurnValidation parseValidation(const std::string& json) {
    CheckTurnValidation validation;
    validation.valid = extractBool(json, "valid", true);
    validation.activeKingInCheck = extractBool(json, "activeKingInCheck", false);
    validation.projectedKingInCheck = extractBool(json, "projectedKingInCheck", false);
    validation.hasAnyLegalResponse = extractBool(json, "hasAnyLegalResponse", false);
    validation.requiresSingleResponseMove = extractBool(json, "requiresSingleResponseMove", false);
    validation.hasQueuedMove = extractBool(json, "hasQueuedMove", false);
    validation.bankrupt = extractBool(json, "bankrupt", false);
    validation.projectedEndingGold = extractInt(json, "projectedEndingGold", 0);
    validation.errorMessage = extractString(json, "errorMessage");
    return validation;
}

std::string serializeTurnCommand(const TurnCommand& command) {
    std::ostringstream output;
    output << "{"
           << "\"type\":" << static_cast<int>(command.type) << ","
           << "\"typeKey\":\"" << turnCommandTypeKeyName(command.type) << "\","
           << "\"typeLabel\":\"" << turnCommandTypeLabelName(command.type) << "\","
           << "\"pieceId\":" << command.pieceId << ","
           << "\"pieceTypeContextKey\":\"" << pieceTypeKeyName(command.produceType) << "\","
           << "\"originX\":" << command.origin.x << ","
           << "\"originY\":" << command.origin.y << ","
           << "\"origin\":" << serializeCellPosition(command.origin) << ","
           << "\"destinationX\":" << command.destination.x << ","
           << "\"destinationY\":" << command.destination.y << ","
           << "\"destination\":" << serializeCellPosition(command.destination) << ","
           << "\"buildId\":" << command.buildId << ","
           << "\"buildingType\":" << static_cast<int>(command.buildingType) << ","
           << "\"buildingTypeKey\":\"" << buildingTypeKeyName(command.buildingType) << "\","
           << "\"buildingTypeLabel\":\"" << buildingTypeLabelName(command.buildingType) << "\","
           << "\"buildOriginX\":" << command.buildOrigin.x << ","
           << "\"buildOriginY\":" << command.buildOrigin.y << ","
           << "\"buildOrigin\":" << serializeCellPosition(command.buildOrigin) << ","
           << "\"buildRotationQuarterTurns\":" << command.buildRotationQuarterTurns << ","
           << "\"barracksId\":" << command.barracksId << ","
           << "\"produceType\":" << static_cast<int>(command.produceType) << ","
           << "\"produceTypeKey\":\"" << pieceTypeKeyName(command.produceType) << "\","
           << "\"produceTypeLabel\":\"" << pieceTypeLabelName(command.produceType) << "\","
           << "\"upgradePieceId\":" << command.upgradePieceId << ","
           << "\"upgradeTarget\":" << static_cast<int>(command.upgradeTarget) << ","
           << "\"upgradeTargetKey\":\"" << pieceTypeKeyName(command.upgradeTarget) << "\","
           << "\"upgradeTargetLabel\":\"" << pieceTypeLabelName(command.upgradeTarget) << "\","
           << "\"formationId\":" << command.formationId
           << "}";
    return output.str();
}

TurnCommand parseTurnCommand(const std::string& json) {
    TurnCommand command{};
    command.type = static_cast<TurnCommand::Type>(extractInt(json, "type", 0));
    command.pieceId = extractInt(json, "pieceId", -1);
    command.origin.x = extractInt(json, "originX", 0);
    command.origin.y = extractInt(json, "originY", 0);
    command.destination.x = extractInt(json, "destinationX", 0);
    command.destination.y = extractInt(json, "destinationY", 0);
    command.buildId = extractInt(json, "buildId", -1);
    command.buildingType = static_cast<BuildingType>(extractInt(json, "buildingType", static_cast<int>(BuildingType::Barracks)));
    command.buildOrigin.x = extractInt(json, "buildOriginX", 0);
    command.buildOrigin.y = extractInt(json, "buildOriginY", 0);
    command.buildRotationQuarterTurns = extractInt(json, "buildRotationQuarterTurns", 0);
    command.barracksId = extractInt(json, "barracksId", -1);
    command.produceType = static_cast<PieceType>(extractInt(json, "produceType", static_cast<int>(PieceType::Pawn)));
    command.upgradePieceId = extractInt(json, "upgradePieceId", -1);
    command.upgradeTarget = static_cast<PieceType>(extractInt(json, "upgradeTarget", static_cast<int>(PieceType::Knight)));
    command.formationId = extractInt(json, "formationId", -1);
    return command;
}

std::string serializeTurnCommandAuditEntry(const TurnCommandAuditEntry& entry) {
    std::ostringstream output;
    output << "{"
           << "\"sequence\":" << entry.sequence << ","
           << "\"turnNumber\":" << entry.turnNumber << ","
           << "\"turnElapsedMs\":" << entry.turnElapsedMs << ","
           << "\"action\":" << static_cast<int>(entry.action) << ","
           << "\"actionKey\":\"" << turnCommandAuditActionKeyName(entry.action) << "\","
           << "\"actionLabel\":\"" << turnCommandAuditActionLabelName(entry.action) << "\","
           << "\"accepted\":" << (entry.accepted ? "true" : "false") << ","
           << "\"hasCommand\":" << (entry.hasCommand ? "true" : "false") << ","
           << "\"reason\":\"" << escapeJsonString(entry.reason) << "\",";
    output << "\"command\":";
    if (entry.hasCommand) {
        output << serializeTurnCommand(entry.command);
    } else {
        output << "null";
    }
    output << "}";
    return output.str();
}

TurnCommandAuditEntry parseTurnCommandAuditEntry(const std::string& json) {
    TurnCommandAuditEntry entry;
    entry.sequence = extractInt(json, "sequence", 0);
    entry.turnNumber = extractInt(json, "turnNumber", 0);
    entry.turnElapsedMs = extractLongLong(json, "turnElapsedMs", 0);
    entry.action = static_cast<TurnCommandAuditAction>(extractInt(json, "action", 0));
    entry.accepted = extractBool(json, "accepted", false);
    entry.hasCommand = extractBool(json, "hasCommand", false);
    entry.reason = extractString(json, "reason");
    const std::string commandSection = extractSection(json, "command");
    if (entry.hasCommand && !commandSection.empty()) {
        entry.command = parseTurnCommand(commandSection);
    }
    return entry;
}

std::string serializeBehavioralTelemetryEvent(const BehavioralTelemetryEvent& event) {
    std::ostringstream output;
    output << "{"
           << "\"sequence\":" << event.sequence << ","
           << "\"turnNumber\":" << event.turnNumber << ","
           << "\"activeKingdom\":" << static_cast<int>(event.activeKingdom) << ","
           << "\"activeKingdomKey\":\"" << kingdomKeyName(event.activeKingdom) << "\","
           << "\"origin\":" << static_cast<int>(event.origin) << ","
           << "\"originKey\":\"" << behavioralTelemetryOriginKey(event.origin) << "\","
           << "\"originLabel\":\"" << behavioralTelemetryOriginLabel(event.origin) << "\","
           << "\"stage\":" << static_cast<int>(event.stage) << ","
           << "\"stageKey\":\"" << behavioralTelemetryStageKey(event.stage) << "\","
           << "\"stageLabel\":\"" << behavioralTelemetryStageLabel(event.stage) << "\","
           << "\"eventKey\":\"" << escapeJsonString(event.eventKey) << "\","
           << "\"eventLabel\":\"" << escapeJsonString(event.eventLabel) << "\","
           << "\"turnElapsedMs\":" << event.turnElapsedMs << ","
           << "\"hostObservedAtUnixMs\":" << event.hostObservedAtUnixMs << ","
           << "\"accepted\":" << (event.accepted ? "true" : "false") << ","
           << "\"hasAccepted\":" << (event.hasAccepted ? "true" : "false") << ","
           << "\"hasCell\":" << (event.hasCell ? "true" : "false") << ","
           << "\"cell\":" << serializeCellPosition(event.cell) << ","
           << "\"pieceId\":" << event.pieceId << ","
           << "\"buildId\":" << event.buildId << ","
           << "\"commandAuditSequence\":" << event.commandAuditSequence << ","
           << "\"pendingStateRevision\":" << event.pendingStateRevision << ","
           << "\"reason\":\"" << escapeJsonString(event.reason) << "\""
           << "}";
    return output.str();
}

BehavioralTelemetryEvent parseBehavioralTelemetryEvent(const std::string& json) {
    BehavioralTelemetryEvent event;
    event.sequence = extractInt(json, "sequence", 0);
    event.turnNumber = extractInt(json, "turnNumber", 0);
    event.activeKingdom = static_cast<KingdomId>(extractInt(
        json,
        "activeKingdom",
        static_cast<int>(KingdomId::White)));
    event.origin = static_cast<BehavioralTelemetryOrigin>(extractInt(
        json,
        "origin",
        static_cast<int>(BehavioralTelemetryOrigin::LocalHost)));
    event.stage = static_cast<BehavioralTelemetryStage>(extractInt(
        json,
        "stage",
        static_cast<int>(BehavioralTelemetryStage::Interaction)));
    event.eventKey = extractString(json, "eventKey");
    event.eventLabel = extractString(json, "eventLabel");
    event.turnElapsedMs = extractLongLong(json, "turnElapsedMs", 0);
    event.hostObservedAtUnixMs = extractLongLong(json, "hostObservedAtUnixMs", 0);
    event.accepted = extractBool(json, "accepted", false);
    event.hasAccepted = extractBool(json, "hasAccepted", false);
    event.hasCell = extractBool(json, "hasCell", false);
    const std::string cellSection = extractSection(json, "cell");
    if (!cellSection.empty()) {
        event.cell.x = extractInt(cellSection, "x", 0);
        event.cell.y = extractInt(cellSection, "y", 0);
    }
    event.pieceId = extractInt(json, "pieceId", -1);
    event.buildId = extractInt(json, "buildId", -1);
    event.commandAuditSequence = extractInt(json, "commandAuditSequence", -1);
    event.pendingStateRevision = extractUInt64(json, "pendingStateRevision", 0);
    event.reason = extractString(json, "reason");
    return event;
}

std::string serializeBehavioralPendingTurnTelemetry(
    const BehavioralPendingTurnTelemetry& pendingTurnTelemetry) {
    std::ostringstream output;
    output << "{";
    output << "\"turnNumber\":" << pendingTurnTelemetry.turnNumber << ",";
    output << "\"activeKingdom\":" << static_cast<int>(pendingTurnTelemetry.activeKingdom) << ",";
    output << "\"activeKingdomKey\":\""
           << kingdomKeyName(pendingTurnTelemetry.activeKingdom) << "\",";
    output << "\"pendingStateRevision\":" << pendingTurnTelemetry.pendingStateRevision << ",";
    output << "\"telemetryRevision\":" << pendingTurnTelemetry.telemetryRevision << ",";
    output << "\"interactionTimeline\":[";
    for (std::size_t index = 0; index < pendingTurnTelemetry.interactionTimeline.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        output << serializeBehavioralTelemetryEvent(
            pendingTurnTelemetry.interactionTimeline[index]);
    }
    output << "],";
    output << "\"orchestrationEvents\":[";
    for (std::size_t index = 0; index < pendingTurnTelemetry.orchestrationEvents.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        output << serializeBehavioralTelemetryEvent(
            pendingTurnTelemetry.orchestrationEvents[index]);
    }
    output << "]";
    output << "}";
    return output.str();
}

BehavioralPendingTurnTelemetry parseBehavioralPendingTurnTelemetry(const std::string& json) {
    BehavioralPendingTurnTelemetry pendingTurnTelemetry;
    if (json.empty()) {
        return pendingTurnTelemetry;
    }

    pendingTurnTelemetry.turnNumber = extractInt(json, "turnNumber", 0);
    pendingTurnTelemetry.activeKingdom = static_cast<KingdomId>(extractInt(
        json,
        "activeKingdom",
        static_cast<int>(KingdomId::White)));
    pendingTurnTelemetry.pendingStateRevision = extractUInt64(json, "pendingStateRevision", 0);
    pendingTurnTelemetry.telemetryRevision = extractUInt64(json, "telemetryRevision", 0);

    for (const std::string& element : splitArrayElements(extractArray(json, "interactionTimeline"))) {
        pendingTurnTelemetry.interactionTimeline.push_back(parseBehavioralTelemetryEvent(element));
    }
    for (const std::string& element : splitArrayElements(extractArray(json, "orchestrationEvents"))) {
        pendingTurnTelemetry.orchestrationEvents.push_back(parseBehavioralTelemetryEvent(element));
    }

    return pendingTurnTelemetry;
}

std::string serializeXPRewardAuditEntry(const XPRewardAuditEntry& entry) {
    std::ostringstream output;
    output << "{"
           << "\"sequence\":" << entry.sequence << ","
           << "\"source\":" << static_cast<int>(entry.source) << ","
           << "\"sourceKey\":\"" << xpRewardSourceKeyName(entry.source) << "\"," 
           << "\"sourceLabel\":\"" << xpRewardSourceLabelName(entry.source) << "\"," 
           << "\"amount\":" << entry.amount << ","
           << "\"recipientPieceId\":" << entry.recipientPieceId << ","
           << "\"recipientPieceType\":" << static_cast<int>(entry.recipientPieceType) << ","
           << "\"recipientPieceTypeKey\":\"" << pieceTypeKeyName(entry.recipientPieceType) << "\"," 
           << "\"recipientPieceTypeLabel\":\"" << pieceTypeLabelName(entry.recipientPieceType) << "\"," 
           << "\"recipientKingdom\":" << static_cast<int>(entry.recipientKingdom) << ","
           << "\"recipientKingdomKey\":\"" << kingdomKeyName(entry.recipientKingdom) << "\"," 
           << "\"recipientKingdomLabel\":\"" << kingdomLabelName(entry.recipientKingdom) << "\"," 
           << "\"recipientCellX\":" << entry.recipientCellX << ","
           << "\"recipientCellY\":" << entry.recipientCellY << ","
           << "\"recipientXpBefore\":" << entry.recipientXpBefore << ","
           << "\"recipientXpAfter\":" << entry.recipientXpAfter << ","
           << "\"hasVictimPieceType\":" << (entry.hasVictimPieceType ? "true" : "false") << ","
           << "\"victimPieceType\":" << static_cast<int>(entry.victimPieceType) << ","
           << "\"victimPieceTypeKey\":\"" << pieceTypeKeyName(entry.victimPieceType) << "\"," 
           << "\"victimPieceTypeLabel\":\"" << pieceTypeLabelName(entry.victimPieceType) << "\"," 
           << "\"rngCounterBefore\":" << entry.rngCounterBefore << ","
           << "\"rngCounterAfter\":" << entry.rngCounterAfter
           << "}";
    return output.str();
}

XPRewardAuditEntry parseXPRewardAuditEntry(const std::string& json) {
    XPRewardAuditEntry entry;
    entry.sequence = extractInt(json, "sequence", 0);
    entry.source = static_cast<XPRewardSource>(extractInt(json, "source", static_cast<int>(XPRewardSource::DestroyBlock)));
    entry.amount = extractInt(json, "amount", 0);
    entry.recipientPieceId = extractInt(json, "recipientPieceId", -1);
    entry.recipientPieceType = static_cast<PieceType>(extractInt(json, "recipientPieceType", static_cast<int>(PieceType::Pawn)));
    entry.recipientKingdom = static_cast<KingdomId>(extractInt(json, "recipientKingdom", static_cast<int>(KingdomId::White)));
    entry.recipientCellX = extractInt(json, "recipientCellX", 0);
    entry.recipientCellY = extractInt(json, "recipientCellY", 0);
    entry.recipientXpBefore = extractInt(json, "recipientXpBefore", 0);
    entry.recipientXpAfter = extractInt(json, "recipientXpAfter", 0);
    entry.hasVictimPieceType = extractBool(json, "hasVictimPieceType", false);
    entry.victimPieceType = static_cast<PieceType>(extractInt(json, "victimPieceType", static_cast<int>(PieceType::Pawn)));
    entry.rngCounterBefore = static_cast<std::uint32_t>(extractInt(json, "rngCounterBefore", 0));
    entry.rngCounterAfter = static_cast<std::uint32_t>(extractInt(json, "rngCounterAfter", 0));
    return entry;
}

std::string serializeNotification(const GameplayNotification& notification) {
    std::ostringstream output;
    output << "{"
           << "\"kind\":" << static_cast<int>(notification.kind) << ","
           << "\"kindKey\":\"" << gameplayNotificationKindKeyName(notification.kind) << "\","
           << "\"kindLabel\":\"" << gameplayNotificationKindLabelName(notification.kind) << "\","
           << "\"kingdom\":" << static_cast<int>(notification.kingdom) << ","
           << "\"kingdomKey\":\"" << kingdomKeyName(notification.kingdom) << "\","
           << "\"kingdomLabel\":\"" << kingdomLabelName(notification.kingdom) << "\","
           << "\"chestRewardType\":" << static_cast<int>(notification.chestReward.type) << ","
           << "\"chestRewardTypeKey\":\"" << chestRewardTypeKeyName(notification.chestReward.type) << "\","
           << "\"chestRewardAmount\":" << notification.chestReward.amount << ","
           << "\"chestReward\":" << serializeChestReward(notification.chestReward) << ","
           << "\"title\":\"" << escapeJsonString(gameplayNotificationTitle(notification)) << "\","
           << "\"message\":\"" << escapeJsonString(gameplayNotificationMessage(notification)) << "\""
           << "}";
    return output.str();
}

GameplayNotification parseNotification(const std::string& json) {
    GameplayNotification notification;
    notification.kind = static_cast<GameplayNotificationKind>(extractInt(json, "kind", 0));
    notification.kingdom = static_cast<KingdomId>(extractInt(json, "kingdom", static_cast<int>(KingdomId::White)));
    notification.chestReward.type = static_cast<ChestRewardType>(extractInt(json, "chestRewardType", static_cast<int>(ChestRewardType::Gold)));
    notification.chestReward.amount = extractInt(json, "chestRewardAmount", 0);
    return notification;
}

std::string serializePieceEntry(const Piece& piece, const WeatherMaskCache& weatherMaskCache) {
    const bool hiddenFromWhite = WeatherVisibility::shouldHidePiece(
        piece,
        KingdomId::White,
        weatherMaskCache);
    const bool hiddenFromBlack = WeatherVisibility::shouldHidePiece(
        piece,
        KingdomId::Black,
        weatherMaskCache);

    std::ostringstream output;
    output << "{"
           << "\"id\":" << piece.id << ","
           << "\"pieceTypeId\":" << static_cast<int>(piece.type) << ","
           << "\"pieceTypeKey\":\"" << pieceTypeKeyName(piece.type) << "\","
           << "\"pieceTypeLabel\":\"" << pieceTypeLabelName(piece.type) << "\","
           << "\"kingdomId\":" << static_cast<int>(piece.kingdom) << ","
           << "\"kingdomKey\":\"" << kingdomKeyName(piece.kingdom) << "\","
           << "\"kingdomLabel\":\"" << kingdomLabelName(piece.kingdom) << "\","
           << "\"position\":" << serializeCellPosition(piece.position) << ","
           << "\"xp\":" << piece.xp << ","
           << "\"formationId\":" << piece.formationId << ","
           << "\"hiddenFromWhite\":" << (hiddenFromWhite ? "true" : "false") << ","
           << "\"hiddenFromBlack\":" << (hiddenFromBlack ? "true" : "false")
           << "}";
    return output.str();
}

std::string serializeBuildingCells(const Building& building,
                                   const WeatherMaskCache& weatherMaskCache) {
    std::ostringstream output;
    output << "[";
    const int footprintWidth = building.getFootprintWidth();
    const int footprintHeight = building.getFootprintHeight();
    bool firstCell = true;
    for (int localY = 0; localY < footprintHeight; ++localY) {
        for (int localX = 0; localX < footprintWidth; ++localX) {
            if (!firstCell) {
                output << ",";
            }
            firstCell = false;

            const sf::Vector2i worldCell{building.origin.x + localX, building.origin.y + localY};
            const sf::Vector2i sourceLocal = building.mapFootprintToSourceLocal(localX, localY);
            output << "{"
                   << "\"footprintLocal\":{" << "\"x\":" << localX << ",\"y\":" << localY << "},"
                   << "\"sourceLocal\":{" << "\"x\":" << sourceLocal.x << ",\"y\":" << sourceLocal.y << "},"
                   << "\"worldCell\":" << serializeCellPosition(worldCell) << ","
                   << "\"destroyed\":" << (building.isCellDestroyed(localX, localY) ? "true" : "false") << ","
                   << "\"breached\":" << (building.isCellBreached(localX, localY) ? "true" : "false") << ","
                   << "\"hp\":" << building.getCellHP(localX, localY) << ","
                   << "\"hiddenFromWhite\":"
                   << (WeatherVisibility::shouldHideBuildingCell(building, worldCell, KingdomId::White, weatherMaskCache)
                           ? "true"
                           : "false")
                   << ","
                   << "\"hiddenFromBlack\":"
                   << (WeatherVisibility::shouldHideBuildingCell(building, worldCell, KingdomId::Black, weatherMaskCache)
                           ? "true"
                           : "false")
                   << "}";
        }
    }
    output << "]";
    return output.str();
}

std::string serializeBuildingEntry(const Building& building,
                                   const WeatherMaskCache& weatherMaskCache) {
    const bool hiddenFromWhite = WeatherVisibility::shouldHideBuildingOverlay(
        building,
        KingdomId::White,
        weatherMaskCache);
    const bool hiddenFromBlack = WeatherVisibility::shouldHideBuildingOverlay(
        building,
        KingdomId::Black,
        weatherMaskCache);

    std::ostringstream output;
    output << "{"
           << "\"id\":" << building.id << ","
           << "\"buildingTypeId\":" << static_cast<int>(building.type) << ","
           << "\"buildingTypeKey\":\"" << buildingTypeKeyName(building.type) << "\","
           << "\"buildingTypeLabel\":\"" << buildingTypeLabelName(building.type) << "\","
           << "\"isPublic\":" << (building.isPublic() ? "true" : "false") << ","
           << "\"isNeutral\":" << (building.isNeutral ? "true" : "false") << ","
           << "\"ownerKingdomId\":" << static_cast<int>(building.owner) << ","
           << "\"ownerKingdomKey\":\"" << kingdomKeyName(building.owner) << "\","
           << "\"origin\":" << serializeCellPosition(building.origin) << ","
           << "\"footprintWidth\":" << building.getFootprintWidth() << ","
           << "\"footprintHeight\":" << building.getFootprintHeight() << ","
           << "\"rotationQuarterTurns\":" << building.rotationQuarterTurns << ","
           << "\"flipMask\":" << building.flipMask << ","
           << "\"state\":\"" << (building.isUnderConstruction() ? "under_construction" : "completed") << "\","
           << "\"destroyed\":" << (building.isDestroyed() ? "true" : "false") << ","
           << "\"destroyedCellCount\":" << building.destroyedCellCount() << ","
           << "\"destroyedCellsRequired\":" << building.effectiveDestroyedCellsRequired() << ","
           << "\"isProducing\":" << (building.isProducing ? "true" : "false") << ","
           << "\"producingTypeId\":" << building.producingType << ","
           << "\"turnsRemaining\":" << building.turnsRemaining << ","
           << "\"hiddenFromWhite\":" << (hiddenFromWhite ? "true" : "false") << ","
           << "\"hiddenFromBlack\":" << (hiddenFromBlack ? "true" : "false") << ","
           << "\"cells\":" << serializeBuildingCells(building, weatherMaskCache)
           << "}";
    return output.str();
}

std::string serializeMapObjectEntry(const MapObject& object,
                                    const ChestSystemState& chestSystemState) {
    std::ostringstream output;
    output << "{"
           << "\"id\":" << object.id << ","
           << "\"typeId\":" << static_cast<int>(object.type) << ","
           << "\"typeKey\":\"chest\","
           << "\"typeLabel\":\"" << mapObjectTypeLabel(object.type) << "\","
           << "\"position\":" << serializeCellPosition(object.position) << ","
           << "\"isActiveChest\":"
           << ((chestSystemState.activeChestObjectId == object.id) ? "true" : "false") << ","
           << "\"chest\":{";
    output << "\"spawnTurn\":" << object.chest.spawnTurn << ",";
    output << "\"reward\":" << serializeChestReward(object.chest.reward);
    output << "}"
           << "}";
    return output.str();
}

std::string serializeAutonomousUnitEntry(const AutonomousUnit& unit,
                                         const WeatherMaskCache& weatherMaskCache,
                                         int activeInfernalUnitId) {
    const bool hidden = WeatherVisibility::shouldHideAutonomousUnit(unit, weatherMaskCache);

    std::ostringstream output;
    output << "{"
           << "\"id\":" << unit.id << ","
           << "\"typeId\":" << static_cast<int>(unit.type) << ","
           << "\"typeKey\":\"infernal_piece\","
           << "\"typeLabel\":\"" << autonomousUnitTypeDisplayName(unit.type) << "\","
           << "\"position\":" << serializeCellPosition(unit.position) << ","
           << "\"isActiveInfernal\":" << ((activeInfernalUnitId == unit.id) ? "true" : "false") << ","
           << "\"hiddenFromWhite\":" << (hidden ? "true" : "false") << ","
           << "\"hiddenFromBlack\":" << (hidden ? "true" : "false") << ","
           << "\"infernal\":{";
    output << "\"targetKingdomId\":" << static_cast<int>(unit.infernal.targetKingdom) << ",";
    output << "\"targetKingdomKey\":\"" << kingdomKeyName(unit.infernal.targetKingdom) << "\",";
    output << "\"targetPieceId\":" << unit.infernal.targetPieceId << ",";
    output << "\"manifestedPieceTypeId\":" << static_cast<int>(unit.infernal.manifestedPieceType) << ",";
    output << "\"manifestedPieceTypeKey\":\"" << pieceTypeKeyName(unit.infernal.manifestedPieceType) << "\",";
    output << "\"preferredTargetTypeId\":" << static_cast<int>(unit.infernal.preferredTargetType) << ",";
    output << "\"preferredTargetTypeKey\":\"" << pieceTypeKeyName(unit.infernal.preferredTargetType) << "\",";
    output << "\"phaseId\":" << static_cast<int>(unit.infernal.phase) << ",";
    output << "\"phaseLabel\":\"" << infernalPhaseDisplayName(unit.infernal.phase) << "\",";
    output << "\"returnBorderCell\":" << serializeCellPosition(unit.infernal.returnBorderCell) << ",";
    output << "\"spawnTurn\":" << unit.infernal.spawnTurn;
    output << "}"
           << "}";
    return output.str();
}

std::string serializePieceIndex(const SaveData& snapshot) {
    std::ostringstream output;
    output << "[";
    bool firstPiece = true;
    for (KingdomId kingdom : kAllKingdoms) {
        for (const Piece& piece : snapshot.kingdoms[kingdomIndex(kingdom)].pieces) {
            if (!firstPiece) {
                output << ",";
            }
            firstPiece = false;
            output << serializePieceEntry(piece, snapshot.weatherMaskCache);
        }
    }
    output << "]";
    return output.str();
}

std::string serializeBuildingIndex(const SaveData& snapshot) {
    std::ostringstream output;
    output << "[";
    bool firstBuilding = true;
    for (KingdomId kingdom : kAllKingdoms) {
        for (const Building& building : snapshot.kingdoms[kingdomIndex(kingdom)].buildings) {
            if (!firstBuilding) {
                output << ",";
            }
            firstBuilding = false;
            output << serializeBuildingEntry(building, snapshot.weatherMaskCache);
        }
    }
    for (const Building& building : snapshot.publicBuildings) {
        if (!firstBuilding) {
            output << ",";
        }
        firstBuilding = false;
        output << serializeBuildingEntry(building, snapshot.weatherMaskCache);
    }
    output << "]";
    return output.str();
}

std::string serializeMapObjectIndex(const SaveData& snapshot) {
    std::ostringstream output;
    output << "[";
    for (std::size_t index = 0; index < snapshot.mapObjects.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        output << serializeMapObjectEntry(snapshot.mapObjects[index], snapshot.chestSystemState);
    }
    output << "]";
    return output.str();
}

std::string serializeAutonomousUnitIndex(const SaveData& snapshot) {
    std::ostringstream output;
    output << "[";
    for (std::size_t index = 0; index < snapshot.autonomousUnits.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        output << serializeAutonomousUnitEntry(
            snapshot.autonomousUnits[index],
            snapshot.weatherMaskCache,
            snapshot.infernalSystemState.activeInfernalUnitId);
    }
    output << "]";
    return output.str();
}

std::string serializeEconomyAnalytics(const SaveData& snapshot,
                                      const GameConfig& config) {
    std::ostringstream output;
    output << "{";
    output << "\"byKingdom\":[";
    for (std::size_t index = 0; index < kAllKingdoms.size(); ++index) {
        if (index > 0) {
            output << ",";
        }

        const KingdomId kingdom = kAllKingdoms[index];
        const SaveData::KingdomData& kingdomData = snapshot.kingdoms[kingdomIndex(kingdom)];
        const TurnEconomyBreakdown breakdown = calculateSnapshotTurnEconomy(snapshot, kingdom, config);
        output << "{"
               << "\"kingdomId\":" << static_cast<int>(kingdom) << ","
               << "\"kingdomKey\":\"" << kingdomKeyName(kingdom) << "\","
               << "\"gold\":" << kingdomData.gold << ","
               << "\"movementPointsMaxBonus\":" << kingdomData.movementPointsMaxBonus << ","
               << "\"buildPointsMaxBonus\":" << kingdomData.buildPointsMaxBonus << ","
               << "\"grossIncome\":" << breakdown.grossIncome << ","
               << "\"upkeepCost\":" << breakdown.upkeepCost << ","
               << "\"netIncome\":" << breakdown.netIncome << ","
               << "\"projectedEndingGold\":" << breakdown.endingGold << ","
               << "\"wouldBeBankrupt\":" << (breakdown.wouldBeBankrupt() ? "true" : "false") << ","
               << "\"pieceCountsByType\":" << serializePieceTypeCounts(kingdomData.pieces) << ","
               << "\"buildingCountsByType\":" << serializeBuildingTypeCounts(kingdomData.buildings)
               << "}";
    }
    output << "],";

    output << "\"publicResourceBuildings\":[";
    bool firstBuilding = true;
    for (const Building& building : snapshot.publicBuildings) {
        const ResourceIncomeBreakdown breakdown =
            calculateSnapshotResourceIncomeBreakdown(snapshot, building, config);
        if (!breakdown.isResourceBuilding) {
            continue;
        }

        if (!firstBuilding) {
            output << ",";
        }
        firstBuilding = false;
        output << "{"
               << "\"buildingId\":" << building.id << ","
               << "\"buildingTypeId\":" << static_cast<int>(building.type) << ","
               << "\"buildingTypeKey\":\"" << buildingTypeKeyName(building.type) << "\","
               << "\"buildingTypeLabel\":\"" << buildingTypeLabelName(building.type) << "\","
               << "\"whiteOccupiedCells\":" << breakdown.whiteOccupiedCells << ","
               << "\"blackOccupiedCells\":" << breakdown.blackOccupiedCells << ","
               << "\"whiteIncome\":" << breakdown.whiteIncome << ","
               << "\"blackIncome\":" << breakdown.blackIncome
               << "}";
    }
    output << "]";
    output << "}";
    return output.str();
}

std::string serializeVisibilityAnalytics(const SaveData& snapshot) {
    int fogCellCount = 0;
    int concealingFogCellCount = 0;
    int maxAlpha = 0;
    int maxShade = 0;
    long long alphaSum = 0;
    long long shadeSum = 0;
    for (int y = 0; y < snapshot.weatherMaskCache.diameter; ++y) {
        for (int x = 0; x < snapshot.weatherMaskCache.diameter; ++x) {
            const std::uint8_t alpha = snapshot.weatherMaskCache.alphaByCell[static_cast<std::size_t>((y * snapshot.weatherMaskCache.diameter) + x)];
            const std::uint8_t shade = snapshot.weatherMaskCache.shadeByCell[static_cast<std::size_t>((y * snapshot.weatherMaskCache.diameter) + x)];
            if (alpha > 0) {
                ++fogCellCount;
                alphaSum += alpha;
                shadeSum += shade;
                maxAlpha = std::max(maxAlpha, static_cast<int>(alpha));
                maxShade = std::max(maxShade, static_cast<int>(shade));
            }
            if (WeatherVisibility::cellHasConcealingFog(snapshot.weatherMaskCache, {x, y})) {
                ++concealingFogCellCount;
            }
        }
    }

    std::ostringstream output;
    output << "{";
    output << "\"maskDiameter\":" << snapshot.weatherMaskCache.diameter << ",";
    output << "\"hasActiveFront\":" << (snapshot.weatherMaskCache.hasActiveFront ? "true" : "false") << ",";
    output << "\"fogCellCount\":" << fogCellCount << ",";
    output << "\"concealingFogCellCount\":" << concealingFogCellCount << ",";
    output << "\"maxAlpha\":" << maxAlpha << ",";
    output << "\"maxShade\":" << maxShade << ",";
    output << "\"averageAlphaOnFogCells\":"
           << (fogCellCount > 0 ? static_cast<int>(alphaSum / fogCellCount) : 0) << ",";
    output << "\"averageShadeOnFogCells\":"
           << (fogCellCount > 0 ? static_cast<int>(shadeSum / fogCellCount) : 0) << ",";
    output << "\"byObserver\":[";
    for (std::size_t observerIndex = 0; observerIndex < kAllKingdoms.size(); ++observerIndex) {
        if (observerIndex > 0) {
            output << ",";
        }
        const KingdomId observer = kAllKingdoms[observerIndex];
        const KingdomId enemy = opponent(observer);

        std::ostringstream hiddenEnemyPieceIds;
        hiddenEnemyPieceIds << "[";
        bool firstHiddenPiece = true;
        for (const Piece& piece : snapshot.kingdoms[kingdomIndex(enemy)].pieces) {
            if (!WeatherVisibility::shouldHidePiece(piece, observer, snapshot.weatherMaskCache)) {
                continue;
            }
            if (!firstHiddenPiece) {
                hiddenEnemyPieceIds << ",";
            }
            firstHiddenPiece = false;
            hiddenEnemyPieceIds << piece.id;
        }
        hiddenEnemyPieceIds << "]";

        std::ostringstream hiddenEnemyBuildingIds;
        hiddenEnemyBuildingIds << "[";
        bool firstHiddenBuilding = true;
        for (const Building& building : snapshot.kingdoms[kingdomIndex(enemy)].buildings) {
            if (!WeatherVisibility::shouldHideBuildingOverlay(building, observer, snapshot.weatherMaskCache)) {
                continue;
            }
            if (!firstHiddenBuilding) {
                hiddenEnemyBuildingIds << ",";
            }
            firstHiddenBuilding = false;
            hiddenEnemyBuildingIds << building.id;
        }
        hiddenEnemyBuildingIds << "]";

        std::ostringstream hiddenAutonomousUnitIds;
        hiddenAutonomousUnitIds << "[";
        bool firstHiddenAutonomousUnit = true;
        for (const AutonomousUnit& unit : snapshot.autonomousUnits) {
            if (!WeatherVisibility::shouldHideAutonomousUnit(unit, snapshot.weatherMaskCache)) {
                continue;
            }
            if (!firstHiddenAutonomousUnit) {
                hiddenAutonomousUnitIds << ",";
            }
            firstHiddenAutonomousUnit = false;
            hiddenAutonomousUnitIds << unit.id;
        }
        hiddenAutonomousUnitIds << "]";

        output << "{"
               << "\"observerKingdomId\":" << static_cast<int>(observer) << ","
               << "\"observerKingdomKey\":\"" << kingdomKeyName(observer) << "\","
               << "\"hiddenEnemyPieceIds\":" << hiddenEnemyPieceIds.str() << ","
               << "\"hiddenEnemyBuildingIds\":" << hiddenEnemyBuildingIds.str() << ","
               << "\"hiddenAutonomousUnitIds\":" << hiddenAutonomousUnitIds.str()
               << "}";
    }
    output << "]";
    output << "}";
    return output.str();
}

std::string serializeWeatherAnalytics(const SaveData& snapshot) {
    std::ostringstream output;
    output << "{";
    output << "\"nextSpawnTurnStep\":" << snapshot.weatherSystemState.nextSpawnTurnStep << ",";
    output << "\"rngCounter\":" << snapshot.weatherSystemState.rngCounter << ",";
    output << "\"revision\":" << snapshot.weatherSystemState.revision << ",";
    output << "\"frontCount\":" << snapshot.weatherSystemState.activeFronts.size() << ",";
    output << "\"fronts\":[";
    for (std::size_t index = 0; index < snapshot.weatherSystemState.activeFronts.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        output << serializeWeatherFront(snapshot.weatherSystemState.activeFronts[index]);
    }
    output << "]";
    output << "}";
    return output.str();
}

std::string serializeChestAnalytics(const SaveData& snapshot) {
    const ChestLootProgressionState& progression = snapshot.chestSystemState.lootProgression;
    const MapObject* activeChest = findMapObjectById(
        snapshot.mapObjects,
        snapshot.chestSystemState.activeChestObjectId);

    std::ostringstream output;
    output << "{";
    output << "\"activeChestObjectId\":" << snapshot.chestSystemState.activeChestObjectId << ",";
    output << "\"nextSpawnTurn\":" << snapshot.chestSystemState.nextSpawnTurn << ",";
    output << "\"rngCounter\":" << snapshot.chestSystemState.rngCounter << ",";
    output << "\"rewardRngCounter\":" << snapshot.chestSystemState.rewardRngCounter << ",";
    output << "\"lootProgression\":{";
    output << "\"hasCurrentReward\":" << (progression.hasCurrentReward ? "true" : "false") << ",";
    output << "\"currentRewardGeneration\":" << progression.currentRewardGeneration << ",";
    output << "\"currentReward\":" << serializeChestReward(progression.currentReward) << ",";
    output << "\"lastCollectedGenerationByKingdom\":[";
    for (std::size_t index = 0; index < kAllKingdoms.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const KingdomId kingdom = kAllKingdoms[index];
        output << "{"
               << "\"kingdomId\":" << static_cast<int>(kingdom) << ","
               << "\"kingdomKey\":\"" << kingdomKeyName(kingdom) << "\","
               << "\"generation\":" << progression.lastCollectedGenerationByKingdom[kingdomIndex(kingdom)]
               << "}";
    }
    output << "]";
    output << "},";
    output << "\"activeChest\":";
    if (activeChest != nullptr) {
        output << serializeMapObjectEntry(*activeChest, snapshot.chestSystemState);
    } else {
        output << "null";
    }
    output << "}";
    return output.str();
}

std::string serializeInfernalAnalytics(const SaveData& snapshot) {
    const AutonomousUnit* activeInfernal = findAutonomousUnitById(
        snapshot.autonomousUnits,
        snapshot.infernalSystemState.activeInfernalUnitId);

    std::ostringstream output;
    output << "{";
    output << "\"activeInfernalUnitId\":" << snapshot.infernalSystemState.activeInfernalUnitId << ",";
    output << "\"nextSpawnTurn\":" << snapshot.infernalSystemState.nextSpawnTurn << ",";
    output << "\"whiteBloodDebt\":" << snapshot.infernalSystemState.whiteBloodDebt << ",";
    output << "\"blackBloodDebt\":" << snapshot.infernalSystemState.blackBloodDebt << ",";
    output << "\"rngCounter\":" << snapshot.infernalSystemState.rngCounter << ",";
    output << "\"activeInfernal\":";
    if (activeInfernal != nullptr) {
        output << serializeAutonomousUnitEntry(
            *activeInfernal,
            snapshot.weatherMaskCache,
            snapshot.infernalSystemState.activeInfernalUnitId);
    } else {
        output << "null";
    }
    output << "}";
    return output.str();
}

std::string serializeSnapshotAnalytics(const SaveData& snapshot,
                                       const GameConfig& config) {
    std::ostringstream output;
    output << "{";
    output << "\"turnNumber\":" << snapshot.turnNumber << ",";
    output << "\"activeKingdomId\":" << static_cast<int>(snapshot.activeKingdom) << ",";
    output << "\"activeKingdomKey\":\"" << kingdomKeyName(snapshot.activeKingdom) << "\","
           << "\"economy\":" << serializeEconomyAnalytics(snapshot, config) << ","
           << "\"visibility\":" << serializeVisibilityAnalytics(snapshot) << ","
           << "\"weather\":" << serializeWeatherAnalytics(snapshot) << ","
           << "\"chest\":" << serializeChestAnalytics(snapshot) << ","
           << "\"infernal\":" << serializeInfernalAnalytics(snapshot) << ","
           << "\"entities\":{";
    output << "\"pieceIndex\":" << serializePieceIndex(snapshot) << ",";
    output << "\"buildingIndex\":" << serializeBuildingIndex(snapshot) << ",";
    output << "\"mapObjectIndex\":" << serializeMapObjectIndex(snapshot) << ",";
    output << "\"autonomousUnitIndex\":" << serializeAutonomousUnitIndex(snapshot);
    output << "}";
    output << "}";
    return output.str();
}

std::string serializeSnapshotMetrics(const SaveData& snapshot,
                                     const GameConfig& config) {
    int fogCellCount = 0;
    int concealingFogCellCount = 0;
    for (const std::uint8_t alpha : snapshot.weatherMaskCache.alphaByCell) {
        if (alpha > 0) {
            ++fogCellCount;
        }
    }

    for (int y = 0; y < snapshot.weatherMaskCache.diameter; ++y) {
        for (int x = 0; x < snapshot.weatherMaskCache.diameter; ++x) {
            if (WeatherVisibility::cellHasConcealingFog(snapshot.weatherMaskCache, {x, y})) {
                ++concealingFogCellCount;
            }
        }
    }

    int chestCount = 0;
    for (const MapObject& object : snapshot.mapObjects) {
        if (object.type == MapObjectType::Chest) {
            ++chestCount;
        }
    }

    const TurnEconomyBreakdown whiteEconomy =
        calculateSnapshotTurnEconomy(snapshot, KingdomId::White, config);
    const TurnEconomyBreakdown blackEconomy =
        calculateSnapshotTurnEconomy(snapshot, KingdomId::Black, config);
    const int weatherFrontCount = !snapshot.weatherSystemState.activeFronts.empty()
        ? static_cast<int>(snapshot.weatherSystemState.activeFronts.size())
        : (snapshot.weatherSystemState.hasActiveFront ? 1 : 0);

    std::ostringstream output;
    output << "{"
           << "\"turnNumber\":" << snapshot.turnNumber << ","
           << "\"activeKingdomId\":" << static_cast<int>(snapshot.activeKingdom) << ","
           << "\"activeKingdomKey\":\"" << kingdomKeyName(snapshot.activeKingdom) << "\","
           << "\"whiteGold\":" << snapshot.kingdoms[kingdomIndex(KingdomId::White)].gold << ","
           << "\"blackGold\":" << snapshot.kingdoms[kingdomIndex(KingdomId::Black)].gold << ","
           << "\"whiteGrossIncome\":" << whiteEconomy.grossIncome << ","
           << "\"blackGrossIncome\":" << blackEconomy.grossIncome << ","
           << "\"whiteUpkeepCost\":" << whiteEconomy.upkeepCost << ","
           << "\"blackUpkeepCost\":" << blackEconomy.upkeepCost << ","
           << "\"whiteNetIncome\":" << whiteEconomy.netIncome << ","
           << "\"blackNetIncome\":" << blackEconomy.netIncome << ","
           << "\"whitePieceCount\":" << snapshot.kingdoms[kingdomIndex(KingdomId::White)].pieces.size() << ","
           << "\"blackPieceCount\":" << snapshot.kingdoms[kingdomIndex(KingdomId::Black)].pieces.size() << ","
           << "\"whiteBuildingCount\":" << snapshot.kingdoms[kingdomIndex(KingdomId::White)].buildings.size() << ","
           << "\"blackBuildingCount\":" << snapshot.kingdoms[kingdomIndex(KingdomId::Black)].buildings.size() << ","
           << "\"publicBuildingCount\":" << snapshot.publicBuildings.size() << ","
           << "\"mapObjectCount\":" << snapshot.mapObjects.size() << ","
           << "\"chestCount\":" << chestCount << ","
           << "\"autonomousUnitCount\":" << snapshot.autonomousUnits.size() << ","
           << "\"weatherFrontCount\":" << weatherFrontCount << ","
           << "\"fogCellCount\":" << fogCellCount << ","
           << "\"concealingFogCellCount\":" << concealingFogCellCount << ","
           << "\"whiteBloodDebt\":" << snapshot.infernalSystemState.whiteBloodDebt << ","
           << "\"blackBloodDebt\":" << snapshot.infernalSystemState.blackBloodDebt << ","
           << "\"activeInfernalUnitId\":" << snapshot.infernalSystemState.activeInfernalUnitId << ","
           << "\"activeChestObjectId\":" << snapshot.chestSystemState.activeChestObjectId << ","
           << "\"recordedEventCount\":" << snapshot.events.size()
           << "}";
    return output.str();
}

std::string serializeEvent(const EventLog::Event& event) {
    std::ostringstream output;
    output << "{"
           << "\"turnNumber\":" << event.turnNumber << ","
           << "\"kingdom\":" << static_cast<int>(event.kingdom) << ","
           << "\"kingdomKey\":\"" << kingdomKeyName(event.kingdom) << "\","
           << "\"kingdomLabel\":\"" << kingdomLabelName(event.kingdom) << "\","
           << "\"message\":\"" << escapeJsonString(event.message) << "\"," 
           << "\"kind\":" << static_cast<int>(event.kind) << ","
           << "\"kindKey\":\"" << eventKindKeyName(event.kind) << "\","
           << "\"kindLabel\":\"" << eventKindLabelName(event.kind) << "\","
           << "\"pieceType\":" << static_cast<int>(event.pieceType) << ","
           << "\"pieceTypeKey\":\"" << pieceTypeKeyName(event.pieceType) << "\","
           << "\"pieceTypeLabel\":\"" << pieceTypeLabelName(event.pieceType) << "\","
           << "\"destinationX\":" << event.destinationCell.x << ","
           << "\"destinationY\":" << event.destinationCell.y << ","
           << "\"destination\":" << serializeCellPosition(event.destinationCell) << ","
           << "\"destinationHiddenWhite\":"
           << (event.destinationHiddenByKingdom[kingdomIndex(KingdomId::White)] ? "true" : "false") << ","
           << "\"destinationHiddenBlack\":"
           << (event.destinationHiddenByKingdom[kingdomIndex(KingdomId::Black)] ? "true" : "false")
           << "}";
    return output.str();
}

EventLog::Event parseEvent(const std::string& json) {
    EventLog::Event event;
    event.turnNumber = extractInt(json, "turnNumber", 0);
    event.kingdom = static_cast<KingdomId>(extractInt(json, "kingdom", static_cast<int>(KingdomId::White)));
    event.message = extractString(json, "message");
    event.kind = static_cast<EventLog::Event::Kind>(extractInt(json, "kind", 0));
    event.pieceType = static_cast<PieceType>(extractInt(json, "pieceType", static_cast<int>(PieceType::Pawn)));
    event.destinationCell.x = extractInt(json, "destinationX", 0);
    event.destinationCell.y = extractInt(json, "destinationY", 0);
    event.destinationHiddenByKingdom[kingdomIndex(KingdomId::White)] =
        extractBool(json, "destinationHiddenWhite", false);
    event.destinationHiddenByKingdom[kingdomIndex(KingdomId::Black)] =
        extractBool(json, "destinationHiddenBlack", false);
    return event;
}

std::string serializeTurnDelta(const SaveData& previousSnapshot,
                               const GameDataTurnRecord& record,
                               const GameConfig& config) {
    const SaveData& currentSnapshot = record.snapshot;
    const KingdomId committedKingdom = record.committedActiveKingdom;
    const SaveData::KingdomData& previousKingdom = previousSnapshot.kingdoms[kingdomIndex(committedKingdom)];
    const TurnPointBudget turnBudget = TurnPointRules::makeBudget(
        config,
        previousKingdom.movementPointsMaxBonus,
        previousKingdom.buildPointsMaxBonus);

    int movementPointsSpent = 0;
    int buildPointsSpent = 0;
    for (const TurnCommand& command : record.queuedCommands) {
        if (command.type == TurnCommand::Move) {
            const Piece* previousPiece = findPieceById(previousSnapshot, command.pieceId);
            if (previousPiece != nullptr) {
                movementPointsSpent += TurnPointRules::movementCost(previousPiece->type, config);
            }
        } else if (command.type == TurnCommand::Build) {
            buildPointsSpent += TurnPointRules::buildCost(command.buildingType, config);
        }
    }

    const int movementPointsUnused = std::max(0, turnBudget.movementPointsMax - movementPointsSpent);
    const int buildPointsUnused = std::max(0, turnBudget.buildPointsMax - buildPointsSpent);

    std::vector<std::string> kingdomEconomyDeltas;
    for (KingdomId kingdom : kAllKingdoms) {
        const SaveData::KingdomData& before = previousSnapshot.kingdoms[kingdomIndex(kingdom)];
        const SaveData::KingdomData& after = currentSnapshot.kingdoms[kingdomIndex(kingdom)];
        std::ostringstream item;
        item << "{"
             << "\"kingdomId\":" << static_cast<int>(kingdom) << ","
             << "\"kingdomKey\":\"" << kingdomKeyName(kingdom) << "\","
             << "\"goldBefore\":" << before.gold << ","
             << "\"goldAfter\":" << after.gold << ","
             << "\"goldDelta\":" << (after.gold - before.gold) << ","
             << "\"movementPointsMaxBonusBefore\":" << before.movementPointsMaxBonus << ","
             << "\"movementPointsMaxBonusAfter\":" << after.movementPointsMaxBonus << ","
             << "\"movementPointsMaxBonusDelta\":"
             << (after.movementPointsMaxBonus - before.movementPointsMaxBonus) << ","
             << "\"buildPointsMaxBonusBefore\":" << before.buildPointsMaxBonus << ","
             << "\"buildPointsMaxBonusAfter\":" << after.buildPointsMaxBonus << ","
             << "\"buildPointsMaxBonusDelta\":" << (after.buildPointsMaxBonus - before.buildPointsMaxBonus) << ","
             << "\"pieceCountBefore\":" << before.pieces.size() << ","
             << "\"pieceCountAfter\":" << after.pieces.size() << ","
             << "\"pieceCountDelta\":" << (static_cast<int>(after.pieces.size()) - static_cast<int>(before.pieces.size())) << ","
             << "\"buildingCountBefore\":" << before.buildings.size() << ","
             << "\"buildingCountAfter\":" << after.buildings.size() << ","
             << "\"buildingCountDelta\":"
             << (static_cast<int>(after.buildings.size()) - static_cast<int>(before.buildings.size()))
             << "}";
        kingdomEconomyDeltas.push_back(item.str());
    }

    std::vector<std::string> movedPieces;
    std::vector<std::string> spawnedPieces;
    std::vector<std::string> removedPieces;
    std::vector<std::string> upgradedPieces;
    std::vector<std::string> xpChanges;
    std::vector<std::string> formationChanges;

    forEachPieceInSnapshot(currentSnapshot, [&](const Piece& piece) {
        const Piece* previousPiece = findPieceById(previousSnapshot, piece.id);
        if (previousPiece == nullptr) {
            std::ostringstream item;
            item << "{"
                 << "\"pieceId\":" << piece.id << ","
                 << "\"pieceTypeId\":" << static_cast<int>(piece.type) << ","
                 << "\"pieceTypeKey\":\"" << pieceTypeKeyName(piece.type) << "\","
                 << "\"kingdomId\":" << static_cast<int>(piece.kingdom) << ","
                 << "\"kingdomKey\":\"" << kingdomKeyName(piece.kingdom) << "\","
                 << "\"position\":" << serializeCellPosition(piece.position)
                 << "}";
            spawnedPieces.push_back(item.str());
            return;
        }

        if (previousPiece->position != piece.position) {
            std::ostringstream item;
            item << "{"
                 << "\"pieceId\":" << piece.id << ","
                 << "\"pieceTypeId\":" << static_cast<int>(piece.type) << ","
                 << "\"pieceTypeKey\":\"" << pieceTypeKeyName(piece.type) << "\","
                 << "\"kingdomId\":" << static_cast<int>(piece.kingdom) << ","
                 << "\"kingdomKey\":\"" << kingdomKeyName(piece.kingdom) << "\","
                 << "\"from\":" << serializeCellPosition(previousPiece->position) << ","
                 << "\"to\":" << serializeCellPosition(piece.position)
                 << "}";
            movedPieces.push_back(item.str());
        }

        if (previousPiece->type != piece.type) {
            std::ostringstream item;
            item << "{"
                 << "\"pieceId\":" << piece.id << ","
                 << "\"kingdomId\":" << static_cast<int>(piece.kingdom) << ","
                 << "\"kingdomKey\":\"" << kingdomKeyName(piece.kingdom) << "\","
                 << "\"fromPieceTypeId\":" << static_cast<int>(previousPiece->type) << ","
                 << "\"fromPieceTypeKey\":\"" << pieceTypeKeyName(previousPiece->type) << "\","
                 << "\"toPieceTypeId\":" << static_cast<int>(piece.type) << ","
                 << "\"toPieceTypeKey\":\"" << pieceTypeKeyName(piece.type) << "\""
                 << "}";
            upgradedPieces.push_back(item.str());
        }

        if (previousPiece->xp != piece.xp) {
            std::ostringstream item;
            item << "{"
                 << "\"pieceId\":" << piece.id << ","
                 << "\"pieceTypeId\":" << static_cast<int>(piece.type) << ","
                 << "\"pieceTypeKey\":\"" << pieceTypeKeyName(piece.type) << "\","
                 << "\"kingdomId\":" << static_cast<int>(piece.kingdom) << ","
                 << "\"kingdomKey\":\"" << kingdomKeyName(piece.kingdom) << "\","
                 << "\"xpBefore\":" << previousPiece->xp << ","
                 << "\"xpAfter\":" << piece.xp << ","
                 << "\"xpDelta\":" << (piece.xp - previousPiece->xp)
                 << "}";
            xpChanges.push_back(item.str());
        }

        if (previousPiece->formationId != piece.formationId) {
            std::ostringstream item;
            item << "{"
                 << "\"pieceId\":" << piece.id << ","
                 << "\"pieceTypeId\":" << static_cast<int>(piece.type) << ","
                 << "\"pieceTypeKey\":\"" << pieceTypeKeyName(piece.type) << "\","
                 << "\"kingdomId\":" << static_cast<int>(piece.kingdom) << ","
                 << "\"kingdomKey\":\"" << kingdomKeyName(piece.kingdom) << "\","
                 << "\"formationIdBefore\":" << previousPiece->formationId << ","
                 << "\"formationIdAfter\":" << piece.formationId
                 << "}";
            formationChanges.push_back(item.str());
        }
    });

    forEachPieceInSnapshot(previousSnapshot, [&](const Piece& piece) {
        if (findPieceById(currentSnapshot, piece.id) != nullptr) {
            return;
        }

        std::ostringstream item;
        item << "{"
             << "\"pieceId\":" << piece.id << ","
             << "\"pieceTypeId\":" << static_cast<int>(piece.type) << ","
             << "\"pieceTypeKey\":\"" << pieceTypeKeyName(piece.type) << "\","
             << "\"kingdomId\":" << static_cast<int>(piece.kingdom) << ","
             << "\"kingdomKey\":\"" << kingdomKeyName(piece.kingdom) << "\","
             << "\"lastPosition\":" << serializeCellPosition(piece.position) << ","
             << "\"cause\":\"" << pieceRemovalCause(record, piece) << "\""
             << "}";
        removedPieces.push_back(item.str());
    });

    std::vector<std::string> placedBuildings;
    std::vector<std::string> removedBuildings;
    std::vector<std::string> buildingStateChanges;
    std::vector<std::string> productionChanges;
    std::vector<std::string> buildingCellChanges;

    forEachBuildingInSnapshot(currentSnapshot, [&](const Building& building) {
        const Building* previousBuilding = findBuildingById(previousSnapshot, building.id);
        if (previousBuilding == nullptr) {
            std::ostringstream item;
            item << "{"
                 << "\"buildingId\":" << building.id << ","
                 << "\"buildingTypeId\":" << static_cast<int>(building.type) << ","
                 << "\"buildingTypeKey\":\"" << buildingTypeKeyName(building.type) << "\","
                 << "\"ownerKingdomId\":" << static_cast<int>(building.owner) << ","
                 << "\"ownerKingdomKey\":\"" << kingdomKeyName(building.owner) << "\","
                 << "\"isNeutral\":" << (building.isNeutral ? "true" : "false") << ","
                 << "\"origin\":" << serializeCellPosition(building.origin)
                 << "}";
            placedBuildings.push_back(item.str());
            return;
        }

        if (previousBuilding->state != building.state) {
            std::ostringstream item;
            item << "{"
                 << "\"buildingId\":" << building.id << ","
                 << "\"buildingTypeId\":" << static_cast<int>(building.type) << ","
                 << "\"buildingTypeKey\":\"" << buildingTypeKeyName(building.type) << "\","
                 << "\"stateBefore\":\"" << buildingStateKeyName(previousBuilding->state) << "\","
                 << "\"stateAfter\":\"" << buildingStateKeyName(building.state) << "\""
                 << "}";
            buildingStateChanges.push_back(item.str());
        }

        if (previousBuilding->isProducing != building.isProducing
            || previousBuilding->producingType != building.producingType
            || previousBuilding->turnsRemaining != building.turnsRemaining) {
            std::ostringstream item;
            item << "{"
                 << "\"buildingId\":" << building.id << ","
                 << "\"buildingTypeId\":" << static_cast<int>(building.type) << ","
                 << "\"buildingTypeKey\":\"" << buildingTypeKeyName(building.type) << "\","
                 << "\"wasProducing\":" << (previousBuilding->isProducing ? "true" : "false") << ","
                 << "\"isProducing\":" << (building.isProducing ? "true" : "false") << ","
                 << "\"produceTypeBeforeId\":" << previousBuilding->producingType << ","
                 << "\"produceTypeAfterId\":" << building.producingType << ","
                 << "\"produceTypeAfterKey\":\""
                 << pieceTypeKeyName(static_cast<PieceType>(building.producingType)) << "\","
                 << "\"turnsRemainingBefore\":" << previousBuilding->turnsRemaining << ","
                 << "\"turnsRemainingAfter\":" << building.turnsRemaining
                 << "}";
            productionChanges.push_back(item.str());
        }

        const int width = std::min(previousBuilding->getFootprintWidth(), building.getFootprintWidth());
        const int height = std::min(previousBuilding->getFootprintHeight(), building.getFootprintHeight());
        for (int localY = 0; localY < height; ++localY) {
            for (int localX = 0; localX < width; ++localX) {
                const int beforeHp = previousBuilding->getCellHP(localX, localY);
                const int afterHp = building.getCellHP(localX, localY);
                const bool beforeBreached = previousBuilding->isCellBreached(localX, localY);
                const bool afterBreached = building.isCellBreached(localX, localY);
                const bool beforeDestroyed = previousBuilding->isCellDestroyed(localX, localY);
                const bool afterDestroyed = building.isCellDestroyed(localX, localY);

                if (beforeHp == afterHp
                    && beforeBreached == afterBreached
                    && beforeDestroyed == afterDestroyed) {
                    continue;
                }

                const char* changeKind = "changed";
                if (!beforeDestroyed && afterDestroyed) {
                    changeKind = "destroyed";
                } else if (beforeDestroyed && !afterDestroyed) {
                    changeKind = "repaired";
                } else if (afterHp < beforeHp) {
                    changeKind = "damaged";
                } else if (afterHp > beforeHp) {
                    changeKind = "repaired";
                } else if (!beforeBreached && afterBreached) {
                    changeKind = "breached";
                }

                const sf::Vector2i worldCell{building.origin.x + localX, building.origin.y + localY};
                std::ostringstream item;
                item << "{"
                     << "\"buildingId\":" << building.id << ","
                     << "\"buildingTypeId\":" << static_cast<int>(building.type) << ","
                     << "\"buildingTypeKey\":\"" << buildingTypeKeyName(building.type) << "\","
                     << "\"changeKind\":\"" << changeKind << "\","
                     << "\"worldCell\":" << serializeCellPosition(worldCell) << ","
                     << "\"beforeHp\":" << beforeHp << ","
                     << "\"afterHp\":" << afterHp << ","
                     << "\"beforeBreached\":" << (beforeBreached ? "true" : "false") << ","
                     << "\"afterBreached\":" << (afterBreached ? "true" : "false") << ","
                     << "\"beforeDestroyed\":" << (beforeDestroyed ? "true" : "false") << ","
                     << "\"afterDestroyed\":" << (afterDestroyed ? "true" : "false")
                     << "}";
                buildingCellChanges.push_back(item.str());
            }
        }
    });

    forEachBuildingInSnapshot(previousSnapshot, [&](const Building& building) {
        if (findBuildingById(currentSnapshot, building.id) != nullptr) {
            return;
        }

        std::ostringstream item;
        item << "{"
             << "\"buildingId\":" << building.id << ","
             << "\"buildingTypeId\":" << static_cast<int>(building.type) << ","
             << "\"buildingTypeKey\":\"" << buildingTypeKeyName(building.type) << "\","
             << "\"ownerKingdomId\":" << static_cast<int>(building.owner) << ","
             << "\"ownerKingdomKey\":\"" << kingdomKeyName(building.owner) << "\","
             << "\"isNeutral\":" << (building.isNeutral ? "true" : "false") << ","
             << "\"origin\":" << serializeCellPosition(building.origin)
             << "}";
        removedBuildings.push_back(item.str());
    });

    std::vector<std::string> spawnedObjects;
    std::vector<std::string> removedObjects;
    for (const MapObject& object : currentSnapshot.mapObjects) {
        if (findMapObjectById(previousSnapshot.mapObjects, object.id) != nullptr) {
            continue;
        }

        std::ostringstream item;
        item << "{"
             << "\"objectId\":" << object.id << ","
             << "\"objectTypeKey\":\"" << mapObjectTypeKeyName(object.type) << "\","
             << "\"position\":" << serializeCellPosition(object.position) << ","
             << "\"reward\":" << serializeChestReward(object.chest.reward)
             << "}";
        spawnedObjects.push_back(item.str());
    }

    for (const MapObject& object : previousSnapshot.mapObjects) {
        if (findMapObjectById(currentSnapshot.mapObjects, object.id) != nullptr) {
            continue;
        }

        std::ostringstream item;
        item << "{"
             << "\"objectId\":" << object.id << ","
             << "\"objectTypeKey\":\"" << mapObjectTypeKeyName(object.type) << "\","
             << "\"position\":" << serializeCellPosition(object.position) << ","
             << "\"reward\":" << serializeChestReward(object.chest.reward)
             << "}";
        removedObjects.push_back(item.str());
    }

    std::vector<std::string> spawnedAutonomousUnits;
    std::vector<std::string> removedAutonomousUnits;
    std::vector<std::string> movedAutonomousUnits;
    std::vector<std::string> autonomousPhaseChanges;
    std::vector<std::string> autonomousTargetChanges;

    for (const AutonomousUnit& unit : currentSnapshot.autonomousUnits) {
        const AutonomousUnit* previousUnit = findAutonomousUnitById(previousSnapshot.autonomousUnits, unit.id);
        if (previousUnit == nullptr) {
            std::ostringstream item;
            item << "{"
                 << "\"unitId\":" << unit.id << ","
                 << "\"unitTypeKey\":\"" << autonomousUnitTypeKeyName(unit.type) << "\","
                 << "\"position\":" << serializeCellPosition(unit.position) << ","
                 << "\"targetKingdomId\":" << static_cast<int>(unit.infernal.targetKingdom) << ","
                 << "\"targetKingdomKey\":\"" << kingdomKeyName(unit.infernal.targetKingdom) << "\","
                 << "\"targetPieceId\":" << unit.infernal.targetPieceId << ","
                 << "\"phaseKey\":\"" << infernalPhaseKeyName(unit.infernal.phase) << "\""
                 << "}";
            spawnedAutonomousUnits.push_back(item.str());
            continue;
        }

        if (previousUnit->position != unit.position) {
            std::ostringstream item;
            item << "{"
                 << "\"unitId\":" << unit.id << ","
                 << "\"unitTypeKey\":\"" << autonomousUnitTypeKeyName(unit.type) << "\","
                 << "\"from\":" << serializeCellPosition(previousUnit->position) << ","
                 << "\"to\":" << serializeCellPosition(unit.position)
                 << "}";
            movedAutonomousUnits.push_back(item.str());
        }

        if (previousUnit->infernal.phase != unit.infernal.phase) {
            std::ostringstream item;
            item << "{"
                 << "\"unitId\":" << unit.id << ","
                 << "\"phaseBefore\":\"" << infernalPhaseKeyName(previousUnit->infernal.phase) << "\","
                 << "\"phaseAfter\":\"" << infernalPhaseKeyName(unit.infernal.phase) << "\""
                 << "}";
            autonomousPhaseChanges.push_back(item.str());
        }

        if (previousUnit->infernal.targetPieceId != unit.infernal.targetPieceId
            || previousUnit->infernal.targetKingdom != unit.infernal.targetKingdom) {
            std::ostringstream item;
            item << "{"
                 << "\"unitId\":" << unit.id << ","
                 << "\"targetKingdomBeforeId\":" << static_cast<int>(previousUnit->infernal.targetKingdom) << ","
                 << "\"targetKingdomBeforeKey\":\"" << kingdomKeyName(previousUnit->infernal.targetKingdom) << "\","
                 << "\"targetKingdomAfterId\":" << static_cast<int>(unit.infernal.targetKingdom) << ","
                 << "\"targetKingdomAfterKey\":\"" << kingdomKeyName(unit.infernal.targetKingdom) << "\","
                 << "\"targetPieceIdBefore\":" << previousUnit->infernal.targetPieceId << ","
                 << "\"targetPieceIdAfter\":" << unit.infernal.targetPieceId
                 << "}";
            autonomousTargetChanges.push_back(item.str());
        }
    }

    for (const AutonomousUnit& unit : previousSnapshot.autonomousUnits) {
        if (findAutonomousUnitById(currentSnapshot.autonomousUnits, unit.id) != nullptr) {
            continue;
        }

        std::ostringstream item;
        item << "{"
             << "\"unitId\":" << unit.id << ","
             << "\"unitTypeKey\":\"" << autonomousUnitTypeKeyName(unit.type) << "\","
             << "\"lastPosition\":" << serializeCellPosition(unit.position) << ","
             << "\"phaseKey\":\"" << infernalPhaseKeyName(unit.infernal.phase) << "\""
             << "}";
        removedAutonomousUnits.push_back(item.str());
    }

    std::vector<std::string> spawnedWeatherFronts;
    std::vector<std::string> endedWeatherFronts;
    for (const WeatherFrontDescriptor& front : currentSnapshot.weatherSystemState.activeFronts) {
        if (!snapshotHasWeatherFrontIdentity(previousSnapshot, weatherFrontIdentity(front))) {
            spawnedWeatherFronts.push_back(serializeWeatherFront(front));
        }
    }
    const bool previousHasLegacyOnlyFront = previousSnapshot.weatherSystemState.activeFronts.empty()
        && previousSnapshot.weatherSystemState.hasActiveFront;
    if (previousHasLegacyOnlyFront) {
        const WeatherFrontDescriptor& front = previousSnapshot.weatherSystemState.activeFront;
        if (!snapshotHasWeatherFrontIdentity(currentSnapshot, weatherFrontIdentity(front))) {
            endedWeatherFronts.push_back(serializeWeatherFront(front));
        }
    }
    for (const WeatherFrontDescriptor& front : previousSnapshot.weatherSystemState.activeFronts) {
        if (!snapshotHasWeatherFrontIdentity(currentSnapshot, weatherFrontIdentity(front))) {
            endedWeatherFronts.push_back(serializeWeatherFront(front));
        }
    }

    std::ostringstream output;
    output << "{";
    output << "\"turnNumber\":" << record.committedTurnNumber << ",";
    output << "\"activeKingdomId\":" << static_cast<int>(committedKingdom) << ",";
    output << "\"activeKingdomKey\":\"" << kingdomKeyName(committedKingdom) << "\",";
    output << "\"budgets\":{";
    output << "\"movementPointsMax\":" << turnBudget.movementPointsMax << ",";
    output << "\"movementPointsSpent\":" << movementPointsSpent << ",";
    output << "\"movementPointsUnused\":" << movementPointsUnused << ",";
    output << "\"buildPointsMax\":" << turnBudget.buildPointsMax << ",";
    output << "\"buildPointsSpent\":" << buildPointsSpent << ",";
    output << "\"buildPointsUnused\":" << buildPointsUnused;
    output << "},";
    output << "\"economy\":{\"byKingdom\":" << serializeJsonArray(kingdomEconomyDeltas) << "},";
    output << "\"pieces\":{";
    output << "\"moved\":" << serializeJsonArray(movedPieces) << ",";
    output << "\"spawned\":" << serializeJsonArray(spawnedPieces) << ",";
    output << "\"removed\":" << serializeJsonArray(removedPieces) << ",";
    output << "\"upgraded\":" << serializeJsonArray(upgradedPieces) << ",";
    output << "\"xpChanged\":" << serializeJsonArray(xpChanges) << ",";
    output << "\"formationChanged\":" << serializeJsonArray(formationChanges);
    output << "},";
    output << "\"buildings\":{";
    output << "\"placed\":" << serializeJsonArray(placedBuildings) << ",";
    output << "\"removed\":" << serializeJsonArray(removedBuildings) << ",";
    output << "\"stateChanged\":" << serializeJsonArray(buildingStateChanges) << ",";
    output << "\"productionChanged\":" << serializeJsonArray(productionChanges) << ",";
    output << "\"cellChanged\":" << serializeJsonArray(buildingCellChanges);
    output << "},";
    output << "\"objects\":{";
    output << "\"spawned\":" << serializeJsonArray(spawnedObjects) << ",";
    output << "\"removed\":" << serializeJsonArray(removedObjects);
    output << "},";
    output << "\"autonomousUnits\":{";
    output << "\"spawned\":" << serializeJsonArray(spawnedAutonomousUnits) << ",";
    output << "\"removed\":" << serializeJsonArray(removedAutonomousUnits) << ",";
    output << "\"moved\":" << serializeJsonArray(movedAutonomousUnits) << ",";
    output << "\"phaseChanged\":" << serializeJsonArray(autonomousPhaseChanges) << ",";
    output << "\"targetChanged\":" << serializeJsonArray(autonomousTargetChanges);
    output << "},";
    output << "\"systems\":{";
    output << "\"whiteBloodDebtDelta\":"
           << (currentSnapshot.infernalSystemState.whiteBloodDebt - previousSnapshot.infernalSystemState.whiteBloodDebt) << ",";
    output << "\"blackBloodDebtDelta\":"
           << (currentSnapshot.infernalSystemState.blackBloodDebt - previousSnapshot.infernalSystemState.blackBloodDebt) << ",";
    output << "\"weatherFrontCountDelta\":"
           << (static_cast<int>(currentSnapshot.weatherSystemState.activeFronts.size())
               - static_cast<int>(previousSnapshot.weatherSystemState.activeFronts.size())) << ",";
    output << "\"fogCellCountDelta\":"
           << (static_cast<int>(currentSnapshot.weatherMaskCache.alphaByCell.size())
               - static_cast<int>(previousSnapshot.weatherMaskCache.alphaByCell.size())) << ",";
    output << "\"mapObjectCountDelta\":"
           << (static_cast<int>(currentSnapshot.mapObjects.size()) - static_cast<int>(previousSnapshot.mapObjects.size())) << ",";
    output << "\"autonomousUnitCountDelta\":"
           << (static_cast<int>(currentSnapshot.autonomousUnits.size()) - static_cast<int>(previousSnapshot.autonomousUnits.size())) << ",";
    output << "\"eventCountDelta\":"
           << (static_cast<int>(currentSnapshot.events.size()) - static_cast<int>(previousSnapshot.events.size())) << ",";
    output << "\"chestActiveObjectBefore\":" << previousSnapshot.chestSystemState.activeChestObjectId << ",";
    output << "\"chestActiveObjectAfter\":" << currentSnapshot.chestSystemState.activeChestObjectId << ",";
    output << "\"infernalActiveUnitBefore\":" << previousSnapshot.infernalSystemState.activeInfernalUnitId << ",";
    output << "\"infernalActiveUnitAfter\":" << currentSnapshot.infernalSystemState.activeInfernalUnitId << ",";
    output << "\"weatherFrontsSpawned\":" << serializeJsonArray(spawnedWeatherFronts) << ",";
    output << "\"weatherFrontsEnded\":" << serializeJsonArray(endedWeatherFronts);
    output << "}";
    output << "}";
    return output.str();
}

std::string serializeStructuredEvents(const SaveData& previousSnapshot,
                                      const GameDataTurnRecord& record,
                                      const GameConfig& config) {
    (void) config;
    const SaveData& currentSnapshot = record.snapshot;
    std::vector<std::string> events;
    std::size_t sequence = 0;

    for (const TurnCommandAuditEntry& audit : record.commandAuditTrail) {
        const char* typeKey = "command_attempt";
        const char* typeLabel = "Command Attempt";
        switch (audit.action) {
            case TurnCommandAuditAction::Queue:
                typeKey = audit.accepted ? "command_queued" : "command_rejected";
                typeLabel = audit.accepted ? "Command Queued" : "Command Rejected";
                break;
            case TurnCommandAuditAction::Replace:
                typeKey = audit.accepted ? "command_replaced" : "command_replace_rejected";
                typeLabel = audit.accepted ? "Command Replaced" : "Command Replace Rejected";
                break;
            case TurnCommandAuditAction::Cancel:
                typeKey = audit.accepted ? "command_cancelled" : "command_cancel_rejected";
                typeLabel = audit.accepted ? "Command Cancelled" : "Command Cancel Rejected";
                break;
            case TurnCommandAuditAction::Reset:
                typeKey = "pending_commands_reset";
                typeLabel = "Pending Commands Reset";
                break;
        }

        std::ostringstream item;
        item << "{"
             << "\"sequence\":" << sequence++ << ","
             << "\"typeKey\":\"" << typeKey << "\","
             << "\"typeLabel\":\"" << typeLabel << "\","
             << "\"auditAction\":" << static_cast<int>(audit.action) << ","
             << "\"auditActionKey\":\"" << turnCommandAuditActionKeyName(audit.action) << "\","
             << "\"accepted\":" << (audit.accepted ? "true" : "false") << ","
             << "\"reason\":\"" << escapeJsonString(audit.reason) << "\",";
        item << "\"command\":";
        if (audit.hasCommand) {
            item << serializeTurnCommand(audit.command);
        } else {
            item << "null";
        }
        item << "}";
        events.push_back(item.str());
    }

    for (const TurnCommand& command : record.queuedCommands) {
        std::ostringstream item;
        item << "{"
             << "\"sequence\":" << sequence++ << ","
             << "\"typeKey\":\"command_committed\","
             << "\"typeLabel\":\"Command Committed\","
             << "\"commandTypeId\":" << static_cast<int>(command.type) << ","
             << "\"commandTypeKey\":\"" << turnCommandTypeKeyName(command.type) << "\","
             << "\"commandTypeLabel\":\"" << turnCommandTypeLabelName(command.type) << "\","
             << "\"command\":" << serializeTurnCommand(command)
             << "}";
        events.push_back(item.str());
    }

    for (const XPRewardAuditEntry& audit : record.xpAuditTrail) {
        std::ostringstream item;
        item << "{"
             << "\"sequence\":" << sequence++ << ","
             << "\"typeKey\":\"xp_granted\"," 
             << "\"typeLabel\":\"XP Granted\"," 
             << "\"rewardSourceId\":" << static_cast<int>(audit.source) << ","
             << "\"rewardSourceKey\":\"" << xpRewardSourceKeyName(audit.source) << "\"," 
             << "\"rewardSourceLabel\":\"" << xpRewardSourceLabelName(audit.source) << "\"," 
             << "\"amount\":" << audit.amount << ","
             << "\"recipientPieceId\":" << audit.recipientPieceId << ","
             << "\"recipientPieceTypeId\":" << static_cast<int>(audit.recipientPieceType) << ","
             << "\"recipientPieceTypeKey\":\"" << pieceTypeKeyName(audit.recipientPieceType) << "\"," 
             << "\"recipientKingdomId\":" << static_cast<int>(audit.recipientKingdom) << ","
             << "\"recipientKingdomKey\":\"" << kingdomKeyName(audit.recipientKingdom) << "\"," 
             << "\"recipientPosition\":" << serializeCellPosition({audit.recipientCellX, audit.recipientCellY}) << ","
             << "\"recipientXpBefore\":" << audit.recipientXpBefore << ","
             << "\"recipientXpAfter\":" << audit.recipientXpAfter << ","
             << "\"rngCounterBefore\":" << audit.rngCounterBefore << ","
             << "\"rngCounterAfter\":" << audit.rngCounterAfter << ","
             << "\"hasVictimPieceType\":" << (audit.hasVictimPieceType ? "true" : "false") << ","
             << "\"victimPieceTypeId\":" << static_cast<int>(audit.victimPieceType) << ","
             << "\"victimPieceTypeKey\":\"" << pieceTypeKeyName(audit.victimPieceType) << "\""
             << "}";
        events.push_back(item.str());
    }

    forEachPieceInSnapshot(currentSnapshot, [&](const Piece& piece) {
        const Piece* previousPiece = findPieceById(previousSnapshot, piece.id);
        if (previousPiece == nullptr) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"piece_spawned\","
                 << "\"typeLabel\":\"Piece Spawned\","
                 << "\"pieceId\":" << piece.id << ","
                 << "\"pieceTypeId\":" << static_cast<int>(piece.type) << ","
                 << "\"pieceTypeKey\":\"" << pieceTypeKeyName(piece.type) << "\","
                 << "\"kingdomId\":" << static_cast<int>(piece.kingdom) << ","
                 << "\"kingdomKey\":\"" << kingdomKeyName(piece.kingdom) << "\","
                 << "\"position\":" << serializeCellPosition(piece.position)
                 << "}";
            events.push_back(item.str());
            return;
        }

        if (previousPiece->position != piece.position) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"piece_moved\","
                 << "\"typeLabel\":\"Piece Moved\","
                 << "\"pieceId\":" << piece.id << ","
                 << "\"pieceTypeId\":" << static_cast<int>(piece.type) << ","
                 << "\"pieceTypeKey\":\"" << pieceTypeKeyName(piece.type) << "\","
                 << "\"kingdomId\":" << static_cast<int>(piece.kingdom) << ","
                 << "\"kingdomKey\":\"" << kingdomKeyName(piece.kingdom) << "\","
                 << "\"from\":" << serializeCellPosition(previousPiece->position) << ","
                 << "\"to\":" << serializeCellPosition(piece.position)
                 << "}";
            events.push_back(item.str());
        }

        if (previousPiece->type != piece.type) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"piece_upgraded\","
                 << "\"typeLabel\":\"Piece Upgraded\","
                 << "\"pieceId\":" << piece.id << ","
                 << "\"kingdomId\":" << static_cast<int>(piece.kingdom) << ","
                 << "\"kingdomKey\":\"" << kingdomKeyName(piece.kingdom) << "\","
                 << "\"fromPieceTypeId\":" << static_cast<int>(previousPiece->type) << ","
                 << "\"fromPieceTypeKey\":\"" << pieceTypeKeyName(previousPiece->type) << "\","
                 << "\"toPieceTypeId\":" << static_cast<int>(piece.type) << ","
                 << "\"toPieceTypeKey\":\"" << pieceTypeKeyName(piece.type) << "\""
                 << "}";
            events.push_back(item.str());
        }
    });

    forEachPieceInSnapshot(previousSnapshot, [&](const Piece& piece) {
        if (findPieceById(currentSnapshot, piece.id) != nullptr) {
            return;
        }

        std::ostringstream item;
        item << "{"
             << "\"sequence\":" << sequence++ << ","
             << "\"typeKey\":\"piece_removed\","
             << "\"typeLabel\":\"Piece Removed\","
             << "\"pieceId\":" << piece.id << ","
             << "\"pieceTypeId\":" << static_cast<int>(piece.type) << ","
             << "\"pieceTypeKey\":\"" << pieceTypeKeyName(piece.type) << "\","
             << "\"kingdomId\":" << static_cast<int>(piece.kingdom) << ","
             << "\"kingdomKey\":\"" << kingdomKeyName(piece.kingdom) << "\","
             << "\"lastPosition\":" << serializeCellPosition(piece.position) << ","
             << "\"cause\":\"" << pieceRemovalCause(record, piece) << "\""
             << "}";
        events.push_back(item.str());
    });

    forEachBuildingInSnapshot(currentSnapshot, [&](const Building& building) {
        const Building* previousBuilding = findBuildingById(previousSnapshot, building.id);
        if (previousBuilding == nullptr) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"building_placed\","
                 << "\"typeLabel\":\"Building Placed\","
                 << "\"buildingId\":" << building.id << ","
                 << "\"buildingTypeId\":" << static_cast<int>(building.type) << ","
                 << "\"buildingTypeKey\":\"" << buildingTypeKeyName(building.type) << "\","
                 << "\"origin\":" << serializeCellPosition(building.origin)
                 << "}";
            events.push_back(item.str());
            return;
        }

        if (!previousBuilding->isProducing && building.isProducing) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"production_started\","
                 << "\"typeLabel\":\"Production Started\","
                 << "\"buildingId\":" << building.id << ","
                 << "\"buildingTypeKey\":\"" << buildingTypeKeyName(building.type) << "\","
                 << "\"produceTypeId\":" << building.producingType << ","
                 << "\"produceTypeKey\":\"" << pieceTypeKeyName(static_cast<PieceType>(building.producingType)) << "\""
                 << "}";
            events.push_back(item.str());
        }

        if (previousBuilding->isProducing && !building.isProducing) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"production_completed\","
                 << "\"typeLabel\":\"Production Completed\","
                 << "\"buildingId\":" << building.id << ","
                 << "\"buildingTypeKey\":\"" << buildingTypeKeyName(building.type) << "\","
                 << "\"produceTypeId\":" << previousBuilding->producingType << ","
                 << "\"produceTypeKey\":\""
                 << pieceTypeKeyName(static_cast<PieceType>(previousBuilding->producingType)) << "\""
                 << "}";
            events.push_back(item.str());
        }
    });

    forEachBuildingInSnapshot(previousSnapshot, [&](const Building& building) {
        if (findBuildingById(currentSnapshot, building.id) != nullptr) {
            return;
        }

        std::ostringstream item;
        item << "{"
             << "\"sequence\":" << sequence++ << ","
             << "\"typeKey\":\"building_removed\","
             << "\"typeLabel\":\"Building Removed\","
             << "\"buildingId\":" << building.id << ","
             << "\"buildingTypeId\":" << static_cast<int>(building.type) << ","
             << "\"buildingTypeKey\":\"" << buildingTypeKeyName(building.type) << "\","
             << "\"origin\":" << serializeCellPosition(building.origin)
             << "}";
        events.push_back(item.str());
    });

    for (const MapObject& object : currentSnapshot.mapObjects) {
        if (findMapObjectById(previousSnapshot.mapObjects, object.id) != nullptr) {
            continue;
        }

        std::ostringstream item;
        item << "{"
             << "\"sequence\":" << sequence++ << ","
             << "\"typeKey\":\"chest_spawned\","
             << "\"typeLabel\":\"Chest Spawned\","
             << "\"objectId\":" << object.id << ","
             << "\"position\":" << serializeCellPosition(object.position) << ","
             << "\"reward\":" << serializeChestReward(object.chest.reward)
             << "}";
        events.push_back(item.str());
    }

    for (const GameplayNotification& notification : record.notifications) {
        std::ostringstream item;
        item << "{"
             << "\"sequence\":" << sequence++ << ","
             << "\"typeKey\":\"chest_opened\","
             << "\"typeLabel\":\"Chest Opened\","
             << "\"kingdomId\":" << static_cast<int>(notification.kingdom) << ","
             << "\"kingdomKey\":\"" << kingdomKeyName(notification.kingdom) << "\","
             << "\"reward\":" << serializeChestReward(notification.chestReward)
             << "}";
        events.push_back(item.str());
    }

    for (const AutonomousUnit& unit : currentSnapshot.autonomousUnits) {
        const AutonomousUnit* previousUnit = findAutonomousUnitById(previousSnapshot.autonomousUnits, unit.id);
        if (previousUnit == nullptr) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"infernal_spawned\","
                 << "\"typeLabel\":\"Infernal Spawned\","
                 << "\"unitId\":" << unit.id << ","
                 << "\"position\":" << serializeCellPosition(unit.position) << ","
                 << "\"phaseKey\":\"" << infernalPhaseKeyName(unit.infernal.phase) << "\""
                 << "}";
            events.push_back(item.str());
            continue;
        }

        if (previousUnit->position != unit.position) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"infernal_moved\","
                 << "\"typeLabel\":\"Infernal Moved\","
                 << "\"unitId\":" << unit.id << ","
                 << "\"from\":" << serializeCellPosition(previousUnit->position) << ","
                 << "\"to\":" << serializeCellPosition(unit.position)
                 << "}";
            events.push_back(item.str());
        }

        if (previousUnit->infernal.phase != unit.infernal.phase) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"infernal_phase_changed\","
                 << "\"typeLabel\":\"Infernal Phase Changed\","
                 << "\"unitId\":" << unit.id << ","
                 << "\"phaseBefore\":\"" << infernalPhaseKeyName(previousUnit->infernal.phase) << "\","
                 << "\"phaseAfter\":\"" << infernalPhaseKeyName(unit.infernal.phase) << "\""
                 << "}";
            events.push_back(item.str());
        }
    }

    for (const AutonomousUnit& unit : previousSnapshot.autonomousUnits) {
        if (findAutonomousUnitById(currentSnapshot.autonomousUnits, unit.id) != nullptr) {
            continue;
        }

        std::ostringstream item;
        item << "{"
             << "\"sequence\":" << sequence++ << ","
             << "\"typeKey\":\"infernal_removed\","
             << "\"typeLabel\":\"Infernal Removed\","
             << "\"unitId\":" << unit.id << ","
             << "\"lastPosition\":" << serializeCellPosition(unit.position)
             << "}";
        events.push_back(item.str());
    }

    for (const WeatherFrontDescriptor& front : currentSnapshot.weatherSystemState.activeFronts) {
        if (!snapshotHasWeatherFrontIdentity(previousSnapshot, weatherFrontIdentity(front))) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"weather_front_spawned\","
                 << "\"typeLabel\":\"Weather Front Spawned\","
                 << "\"front\":" << serializeWeatherFront(front)
                 << "}";
            events.push_back(item.str());
        }
    }
    const bool previousHasLegacyOnlyFront = previousSnapshot.weatherSystemState.activeFronts.empty()
        && previousSnapshot.weatherSystemState.hasActiveFront;
    if (previousHasLegacyOnlyFront) {
        const WeatherFrontDescriptor& front = previousSnapshot.weatherSystemState.activeFront;
        if (!snapshotHasWeatherFrontIdentity(currentSnapshot, weatherFrontIdentity(front))) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"weather_front_ended\","
                 << "\"typeLabel\":\"Weather Front Ended\","
                 << "\"front\":" << serializeWeatherFront(front)
                 << "}";
            events.push_back(item.str());
        }
    }
    for (const WeatherFrontDescriptor& front : previousSnapshot.weatherSystemState.activeFronts) {
        if (!snapshotHasWeatherFrontIdentity(currentSnapshot, weatherFrontIdentity(front))) {
            std::ostringstream item;
            item << "{"
                 << "\"sequence\":" << sequence++ << ","
                 << "\"typeKey\":\"weather_front_ended\","
                 << "\"typeLabel\":\"Weather Front Ended\","
                 << "\"front\":" << serializeWeatherFront(front)
                 << "}";
            events.push_back(item.str());
        }
    }

    return serializeJsonArray(events);
}

std::string serializeTurnRecord(const SaveData& previousSnapshot,
                                const GameDataTurnRecord& record,
                                const GameConfig& config,
                                SaveManager& saveManager) {
    std::ostringstream output;
    output << "    {\n";
    output << "      \"committedTurnNumber\": " << record.committedTurnNumber << ",\n";
    output << "      \"committedActiveKingdom\": " << static_cast<int>(record.committedActiveKingdom) << ",\n";
    output << "      \"gameOver\": " << (record.gameOver ? "true" : "false") << ",\n";
    output << "      \"winner\": " << static_cast<int>(record.winner) << ",\n";
    output << "      \"capturedAtUnix\": " << static_cast<long long>(record.capturedAtUnix) << ",\n";
    output << "      \"activeValidation\": " << serializeValidation(record.activeValidation) << ",\n";
    output << "      \"nextTurnValidation\": " << serializeValidation(record.nextTurnValidation) << ",\n";
    output << "      \"queuedCommands\": [";
    for (std::size_t index = 0; index < record.queuedCommands.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        output << serializeTurnCommand(record.queuedCommands[index]);
    }
    output << "],\n";
    output << "      \"commandAuditTrail\": [";
    for (std::size_t index = 0; index < record.commandAuditTrail.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        output << serializeTurnCommandAuditEntry(record.commandAuditTrail[index]);
    }
    output << "],\n";
    output << "      \"xpAuditTrail\": [";
    for (std::size_t index = 0; index < record.xpAuditTrail.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        output << serializeXPRewardAuditEntry(record.xpAuditTrail[index]);
    }
    output << "],\n";
    output << "      \"notifications\": [";
    for (std::size_t index = 0; index < record.notifications.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        output << serializeNotification(record.notifications[index]);
    }
    output << "],\n";
    output << "      \"newEvents\": [";
    for (std::size_t index = 0; index < record.newEvents.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        output << serializeEvent(record.newEvents[index]);
    }
    output << "],\n";
    output << "      \"behavioralTelemetry\": "
           << serializeBehavioralPendingTurnTelemetry(record.behavioralTelemetry) << ",\n";
    output << "      \"turnDelta\": " << serializeTurnDelta(previousSnapshot, record, config) << ",\n";
    output << "      \"structuredEvents\": "
           << serializeStructuredEvents(previousSnapshot, record, config) << ",\n";
    output << "      \"snapshotMetrics\": " << serializeSnapshotMetrics(record.snapshot, config) << ",\n";
    output << "      \"analytics\": " << serializeSnapshotAnalytics(record.snapshot, config) << ",\n";
    output << "      \"snapshot\": ";
    output << indentMultilineJson(saveManager.serialize(record.snapshot), 6) << "\n";
    output << "    }";
    return output.str();
}

GameDataTurnRecord parseTurnRecord(const std::string& json,
                                   SaveManager& saveManager) {
    GameDataTurnRecord record;
    record.committedTurnNumber = extractInt(json, "committedTurnNumber", 0);
    record.committedActiveKingdom = static_cast<KingdomId>(extractInt(json, "committedActiveKingdom", static_cast<int>(KingdomId::White)));
    record.gameOver = extractBool(json, "gameOver", false);
    record.winner = static_cast<KingdomId>(extractInt(json, "winner", static_cast<int>(KingdomId::White)));
    record.capturedAtUnix = static_cast<std::time_t>(extractInt(json, "capturedAtUnix", 0));
    record.activeValidation = parseValidation(extractSection(json, "activeValidation"));
    record.nextTurnValidation = parseValidation(extractSection(json, "nextTurnValidation"));

    for (const std::string& element : splitArrayElements(extractArray(json, "queuedCommands"))) {
        record.queuedCommands.push_back(parseTurnCommand(element));
    }
    for (const std::string& element : splitArrayElements(extractArray(json, "commandAuditTrail"))) {
        record.commandAuditTrail.push_back(parseTurnCommandAuditEntry(element));
    }
    for (const std::string& element : splitArrayElements(extractArray(json, "xpAuditTrail"))) {
        record.xpAuditTrail.push_back(parseXPRewardAuditEntry(element));
    }
    for (const std::string& element : splitArrayElements(extractArray(json, "notifications"))) {
        record.notifications.push_back(parseNotification(element));
    }
    for (const std::string& element : splitArrayElements(extractArray(json, "newEvents"))) {
        record.newEvents.push_back(parseEvent(element));
    }
    record.behavioralTelemetry = parseBehavioralPendingTurnTelemetry(
        extractSection(json, "behavioralTelemetry"));

    SaveData snapshot;
    const std::string snapshotSection = extractSection(json, "snapshot");
    if (!snapshotSection.empty()) {
        saveManager.deserialize(snapshotSection, snapshot);
    }
    record.snapshot = std::move(snapshot);
    return record;
}

void writeError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // namespace

void GameDataRecorder::reset() {
    m_enabled = false;
    m_historyContinuityComplete = true;
    m_loadedFromExistingCompanion = false;
    m_saveName.clear();
    m_createdAtUnix = 0;
    m_lastUpdatedAtUnix = 0;
    m_initialSnapshotReason.clear();
    m_initialSnapshot = SaveData{};
    m_turnHistory.clear();
    m_pendingTurnTelemetry = BehavioralPendingTurnTelemetry{};
    m_lastRecordedEventCount = 0;
}

void GameDataRecorder::bootstrapFromSnapshot(const GameSessionConfig& session,
                                             const SaveData& snapshot,
                                             bool historyContinuityComplete,
                                             const std::string& initialSnapshotReason) {
    reset();
    m_enabled = session.dataCollectionEnabled;
    if (!m_enabled) {
        return;
    }

    m_historyContinuityComplete = historyContinuityComplete;
    m_loadedFromExistingCompanion = false;
    m_saveName = session.saveName;
    m_createdAtUnix = std::time(nullptr);
    m_lastUpdatedAtUnix = m_createdAtUnix;
    m_initialSnapshotReason = initialSnapshotReason;
    m_initialSnapshot = snapshot;
    m_lastRecordedEventCount = snapshot.events.size();
}

void GameDataRecorder::beginNewSession(const GameSessionConfig& session,
                                       const SaveData& initialSnapshot) {
    bootstrapFromSnapshot(session, initialSnapshot, true, "initial_state_new_game");
}

bool GameDataRecorder::loadFromFile(const std::string& dataFilePath,
                                    SaveManager& saveManager) {
    std::ifstream input(dataFilePath);
    if (!input.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string json = buffer.str();
    if (json.empty()) {
        return false;
    }

    if (extractInt(json, "schemaVersion", 0) != kSchemaVersion) {
        return false;
    }

    reset();
    m_enabled = extractBool(json, "dataCollectionEnabled", true);
    m_historyContinuityComplete = extractBool(json, "historyContinuityComplete", false);
    m_loadedFromExistingCompanion = extractBool(json, "loadedFromExistingCompanion", true);
    m_saveName = extractString(json, "saveName");
    m_createdAtUnix = static_cast<std::time_t>(extractInt(json, "createdAtUnix", 0));
    m_lastUpdatedAtUnix = static_cast<std::time_t>(extractInt(json, "lastUpdatedAtUnix", 0));
    m_initialSnapshotReason = extractString(json, "initialSnapshotReason");

    SaveData initialSnapshot;
    const std::string initialSnapshotSection = extractSection(json, "initialSnapshot");
    if (!initialSnapshotSection.empty()) {
        saveManager.deserialize(initialSnapshotSection, initialSnapshot);
    }
    m_initialSnapshot = std::move(initialSnapshot);

    const std::string turnHistoryArray = extractArray(json, "turnHistory");
    for (const std::string& element : splitArrayElements(turnHistoryArray)) {
        m_turnHistory.push_back(parseTurnRecord(element, saveManager));
    }

    m_pendingTurnTelemetry = parseBehavioralPendingTurnTelemetry(
        extractSection(json, "pendingTurnTelemetry"));

    m_lastRecordedEventCount = currentSnapshot().events.size();
    return true;
}

bool GameDataRecorder::resumeOrBootstrapFromSave(const GameSessionConfig& session,
                                                 const SaveData& currentSnapshot,
                                                 const std::string& dataFilePath,
                                                 SaveManager& saveManager) {
    if (!session.dataCollectionEnabled) {
        reset();
        return false;
    }

    if (loadFromFile(dataFilePath, saveManager)) {
        m_enabled = true;
        m_saveName = session.saveName;
        m_lastRecordedEventCount = currentSnapshot.events.size();
        return true;
    }

    bootstrapFromSnapshot(session, currentSnapshot, false, "initial_state_loaded_game_partial");
    return false;
}

void GameDataRecorder::recordCommittedTurn(const std::vector<TurnCommand>& queuedCommands,
                                           const std::vector<TurnCommandAuditEntry>& commandAuditTrail,
                                           const std::vector<XPRewardAuditEntry>& xpAuditTrail,
                                           int committedTurnNumber,
                                           KingdomId committedActiveKingdom,
                                           const CheckTurnValidation& activeValidation,
                                           const CheckTurnValidation& nextTurnValidation,
                                           bool gameOver,
                                           KingdomId winner,
                                           const std::vector<GameplayNotification>& notifications,
                                           const BehavioralPendingTurnTelemetry& behavioralTelemetry,
                                           const SaveData& snapshot) {
    if (!m_enabled) {
        return;
    }

    GameDataTurnRecord record;
    record.committedTurnNumber = committedTurnNumber;
    record.committedActiveKingdom = committedActiveKingdom;
    record.gameOver = gameOver;
    record.winner = winner;
    record.capturedAtUnix = std::time(nullptr);
    record.activeValidation = activeValidation;
    record.nextTurnValidation = nextTurnValidation;
    record.queuedCommands = queuedCommands;
    record.commandAuditTrail = commandAuditTrail;
    record.xpAuditTrail = xpAuditTrail;
    record.notifications = notifications;
    record.behavioralTelemetry = behavioralTelemetry;
    if (m_lastRecordedEventCount < snapshot.events.size()) {
        record.newEvents.assign(snapshot.events.begin() + static_cast<std::ptrdiff_t>(m_lastRecordedEventCount),
                                snapshot.events.end());
    }
    record.snapshot = snapshot;

    m_turnHistory.push_back(std::move(record));
    m_pendingTurnTelemetry = BehavioralPendingTurnTelemetry{};
    m_lastRecordedEventCount = snapshot.events.size();
    m_lastUpdatedAtUnix = std::time(nullptr);
}

void GameDataRecorder::setPendingTurnTelemetry(
    const BehavioralPendingTurnTelemetry& pendingTurnTelemetry) {
    if (!m_enabled) {
        return;
    }

    m_pendingTurnTelemetry = pendingTurnTelemetry;
    m_lastUpdatedAtUnix = std::time(nullptr);
}

const SaveData& GameDataRecorder::currentSnapshot() const {
    if (!m_turnHistory.empty()) {
        return m_turnHistory.back().snapshot;
    }

    return m_initialSnapshot;
}

bool GameDataRecorder::saveToFile(const std::string& dataFilePath,
                                  const GameConfig& config,
                                  SaveManager& saveManager,
                                  std::string* errorMessage) {
    if (!m_enabled) {
        return true;
    }

    std::error_code directoryError;
    fs::create_directories(fs::path(dataFilePath).parent_path(), directoryError);
    if (directoryError) {
        writeError(errorMessage, "Failed to create Data directory.");
        return false;
    }

    std::ofstream output(dataFilePath);
    if (!output.is_open()) {
        writeError(errorMessage, "Failed to write Data companion file.");
        return false;
    }

    m_lastUpdatedAtUnix = std::time(nullptr);

    output << "{\n";
    output << "  \"schemaVersion\": " << kSchemaVersion << ",\n";
    output << "  \"saveName\": \"" << escapeJsonString(m_saveName) << "\",\n";
    output << "  \"dataCollectionEnabled\": " << (m_enabled ? "true" : "false") << ",\n";
    output << "  \"historyContinuityComplete\": "
           << (m_historyContinuityComplete ? "true" : "false") << ",\n";
    output << "  \"loadedFromExistingCompanion\": "
           << (m_loadedFromExistingCompanion ? "true" : "false") << ",\n";
    output << "  \"createdAtUnix\": " << static_cast<long long>(m_createdAtUnix) << ",\n";
    output << "  \"lastUpdatedAtUnix\": " << static_cast<long long>(m_lastUpdatedAtUnix) << ",\n";
    output << "  \"provenance\": " << serializeProvenance(currentSnapshot(), config) << ",\n";
    output << "  \"referenceData\": " << serializeReferenceData() << ",\n";
    output << "  \"sessionContext\": " << serializeSessionContext(currentSnapshot()) << ",\n";
    output << "  \"configContext\": " << serializeConfigContext(config) << ",\n";
    output << "  \"initialSnapshotReason\": \"" << escapeJsonString(m_initialSnapshotReason) << "\",\n";
    output << "  \"initialMetrics\": " << serializeSnapshotMetrics(m_initialSnapshot, config) << ",\n";
    output << "  \"initialAnalytics\": " << serializeSnapshotAnalytics(m_initialSnapshot, config) << ",\n";
    output << "  \"initialSnapshot\": "
           << indentMultilineJson(saveManager.serialize(m_initialSnapshot), 2) << ",\n";
    output << "  \"turnHistory\": [\n";
    const SaveData* previousSnapshot = &m_initialSnapshot;
    for (std::size_t index = 0; index < m_turnHistory.size(); ++index) {
        output << serializeTurnRecord(*previousSnapshot, m_turnHistory[index], config, saveManager);
        if (index + 1 < m_turnHistory.size()) {
            output << ",";
        }
        output << "\n";
        previousSnapshot = &m_turnHistory[index].snapshot;
    }
    output << "  ],\n";
        output << "  \"pendingTurnTelemetry\": "
            << serializeBehavioralPendingTurnTelemetry(m_pendingTurnTelemetry) << ",\n";
    output << "  \"currentMetrics\": " << serializeSnapshotMetrics(currentSnapshot(), config) << ",\n";
    output << "  \"currentAnalytics\": " << serializeSnapshotAnalytics(currentSnapshot(), config) << ",\n";
    output << "  \"currentStateSummary\": "
           << indentMultilineJson(saveManager.serialize(currentSnapshot()), 2) << "\n";
    output << "}\n";
    return true;
}

std::string GameDataRecorder::buildCompanionPath(const std::string& dataDirectory,
                                                 const std::string& saveName) {
    return (fs::path(dataDirectory) / (saveName + ".json")).string();
}

bool GameDataRecorder::deleteCompanion(const std::string& dataDirectory,
                                       const std::string& saveName) {
    const fs::path dataPath = buildCompanionPath(dataDirectory, saveName);
    if (!fs::exists(dataPath)) {
        return true;
    }

    std::error_code error;
    fs::remove(dataPath, error);
    return !error;
}

bool GameDataRecorder::renameCompanion(const std::string& dataDirectory,
                                       const std::string& oldSaveName,
                                       const std::string& newSaveName) {
    if (oldSaveName.empty() || newSaveName.empty() || oldSaveName == newSaveName) {
        return true;
    }

    const fs::path oldPath = buildCompanionPath(dataDirectory, oldSaveName);
    if (!fs::exists(oldPath)) {
        return true;
    }

    const fs::path newPath = buildCompanionPath(dataDirectory, newSaveName);
    std::error_code error;
    fs::create_directories(newPath.parent_path(), error);
    if (error) {
        return false;
    }
    if (fs::exists(newPath)) {
        fs::remove(newPath, error);
        if (error) {
            return false;
        }
    }

    fs::rename(oldPath, newPath, error);
    return !error;
}