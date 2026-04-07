#include "Debug/GameStateDebugRecorder.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
const char* pieceTypeName(PieceType type) {
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

const char* buildingTypeName(BuildingType type) {
    switch (type) {
        case BuildingType::Church: return "Church";
        case BuildingType::Mine: return "Mine";
        case BuildingType::Farm: return "Farm";
        case BuildingType::Barracks: return "Barracks";
        case BuildingType::WoodWall: return "WoodWall";
        case BuildingType::StoneWall: return "StoneWall";
        case BuildingType::Bridge: return "Bridge";
        case BuildingType::Arena: return "Arena";
    }
    return "Unknown";
}

const char* kingdomName(KingdomId id) {
    return id == KingdomId::White ? "White" : "Black";
}

std::string jsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

std::filesystem::path findProjectRoot(std::filesystem::path start) {
    if (start.empty()) {
        start = std::filesystem::current_path();
    }

    for (auto current = start; !current.empty(); current = current.parent_path()) {
        if (std::filesystem::exists(current / "CMakeLists.txt")
            && std::filesystem::exists(current / "assets")
            && std::filesystem::exists(current / "src")) {
            return current;
        }

        if (current == current.root_path()) {
            break;
        }
    }

    return start;
}
}

void GameStateDebugRecorder::reset() {
    m_history.clear();
}

void GameStateDebugRecorder::logTurnState(int turnNumber,
                                          const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                          const std::string& reason) const {
    auto dumpKingdom = [](const Kingdom& kd) {
        std::cout << "  Kingdom " << kingdomName(kd.id)
                  << " | gold=" << kd.gold
                  << " | pieces=" << kd.pieces.size()
                  << " | buildings=" << kd.buildings.size() << '\n';

        std::cout << "    Pieces:" << '\n';
        if (kd.pieces.empty()) {
            std::cout << "      (none)" << '\n';
        } else {
            for (const auto& piece : kd.pieces) {
                std::cout << "      #" << piece.id
                          << " " << pieceTypeName(piece.type)
                          << " @ (" << piece.position.x << "," << piece.position.y << ")"
                          << " xp=" << piece.xp << '\n';
            }
        }

        std::cout << "    Buildings:" << '\n';
        if (kd.buildings.empty()) {
            std::cout << "      (none)" << '\n';
        } else {
            for (const auto& building : kd.buildings) {
                std::cout << "      #" << building.id
                          << " " << buildingTypeName(building.type)
                          << " origin=(" << building.origin.x << "," << building.origin.y << ")"
                          << " size=" << building.width << "x" << building.height;
                if (building.type == BuildingType::Barracks) {
                    std::cout << " producing=" << (building.isProducing ? "yes" : "no")
                              << " turnsRemaining=" << building.turnsRemaining;
                }
                std::cout << '\n';
            }
        }
    };

    std::cout << "\n=== TURN DEBUG | turn=" << turnNumber
              << " | reason=" << reason << " ===" << '\n';
    dumpKingdom(kingdoms[kingdomIndex(KingdomId::White)]);
    dumpKingdom(kingdoms[kingdomIndex(KingdomId::Black)]);
    std::cout << "=== END TURN DEBUG ===" << '\n';
}

void GameStateDebugRecorder::recordSnapshot(int turnNumber,
                                            KingdomId activeKingdom,
                                            const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                            const std::string& reason) {
    DebugTurnSnapshot snapshot;
    snapshot.turnNumber = turnNumber;
    snapshot.activeKingdom = activeKingdom;
    snapshot.reason = reason;
    snapshot.capturedAt = std::time(nullptr);

    for (int index = 0; index < kNumKingdoms; ++index) {
        const Kingdom& source = kingdoms[index];
        DebugKingdomState& target = snapshot.kingdoms[index];
        target.id = source.id;
        target.gold = source.gold;

        target.pieces.reserve(source.pieces.size());
        for (const auto& piece : source.pieces) {
            target.pieces.push_back(DebugPieceState{piece.id, piece.type, piece.position, piece.xp});
        }

        target.buildings.reserve(source.buildings.size());
        for (const auto& building : source.buildings) {
            target.buildings.push_back(DebugBuildingState{
                building.id,
                building.type,
                building.origin,
                building.width,
                building.height,
                building.isProducing,
                building.turnsRemaining
            });
        }
    }

    m_history.push_back(std::move(snapshot));
}

void GameStateDebugRecorder::exportHistory(const std::string& gameName,
                                           int currentTurn,
                                           KingdomId activeKingdom) const {
    try {
        const std::filesystem::path projectRoot = findProjectRoot(std::filesystem::current_path());
        const std::filesystem::path debugDir = projectRoot / "debug_game_state";
        std::filesystem::create_directories(debugDir);

        const std::time_t now = std::time(nullptr);
        const std::string safeGameName = gameName.empty() ? "unnamed_game" : gameName;
        const std::filesystem::path outputPath =
            debugDir / (safeGameName + "_turn_history_" + std::to_string(static_cast<long long>(now)) + ".json");

        std::ofstream out(outputPath);
        if (!out.is_open()) {
            std::cerr << "Failed to write debug game state file: " << outputPath.string() << '\n';
            return;
        }

        out << "{\n";
        out << "  \"game_name\": \"" << jsonEscape(safeGameName) << "\",\n";
        out << "  \"current_turn\": " << currentTurn << ",\n";
        out << "  \"active_kingdom\": \"" << kingdomName(activeKingdom) << "\",\n";
        out << "  \"history\": [\n";

        for (std::size_t snapshotIndex = 0; snapshotIndex < m_history.size(); ++snapshotIndex) {
            const DebugTurnSnapshot& snapshot = m_history[snapshotIndex];
            out << "    {\n";
            out << "      \"turn_number\": " << snapshot.turnNumber << ",\n";
            out << "      \"active_kingdom\": \"" << kingdomName(snapshot.activeKingdom) << "\",\n";
            out << "      \"reason\": \"" << jsonEscape(snapshot.reason) << "\",\n";
            out << "      \"captured_at\": " << static_cast<long long>(snapshot.capturedAt) << ",\n";
            out << "      \"kingdoms\": [\n";

            for (int kingdomIndexValue = 0; kingdomIndexValue < kNumKingdoms; ++kingdomIndexValue) {
                const DebugKingdomState& kingdomState = snapshot.kingdoms[kingdomIndexValue];
                out << "        {\n";
                out << "          \"id\": \"" << kingdomName(kingdomState.id) << "\",\n";
                out << "          \"gold\": " << kingdomState.gold << ",\n";
                out << "          \"pieces\": [\n";

                for (std::size_t pieceIndex = 0; pieceIndex < kingdomState.pieces.size(); ++pieceIndex) {
                    const DebugPieceState& piece = kingdomState.pieces[pieceIndex];
                    out << "            {\n";
                    out << "              \"id\": " << piece.id << ",\n";
                    out << "              \"type\": \"" << pieceTypeName(piece.type) << "\",\n";
                    out << "              \"x\": " << piece.position.x << ",\n";
                    out << "              \"y\": " << piece.position.y << ",\n";
                    out << "              \"xp\": " << piece.xp << "\n";
                    out << "            }" << (pieceIndex + 1 < kingdomState.pieces.size() ? "," : "") << "\n";
                }

                out << "          ],\n";
                out << "          \"buildings\": [\n";

                for (std::size_t buildingIndex = 0; buildingIndex < kingdomState.buildings.size(); ++buildingIndex) {
                    const DebugBuildingState& building = kingdomState.buildings[buildingIndex];
                    out << "            {\n";
                    out << "              \"id\": " << building.id << ",\n";
                    out << "              \"type\": \"" << buildingTypeName(building.type) << "\",\n";
                    out << "              \"x\": " << building.origin.x << ",\n";
                    out << "              \"y\": " << building.origin.y << ",\n";
                    out << "              \"width\": " << building.width << ",\n";
                    out << "              \"height\": " << building.height << ",\n";
                    out << "              \"is_producing\": " << (building.isProducing ? "true" : "false") << ",\n";
                    out << "              \"turns_remaining\": " << building.turnsRemaining << "\n";
                    out << "            }" << (buildingIndex + 1 < kingdomState.buildings.size() ? "," : "") << "\n";
                }

                out << "          ]\n";
                out << "        }" << (kingdomIndexValue + 1 < kNumKingdoms ? "," : "") << "\n";
            }

            out << "      ]\n";
            out << "    }" << (snapshotIndex + 1 < m_history.size() ? "," : "") << "\n";
        }

        out << "  ]\n";
        out << "}\n";
        std::cout << "Debug game history exported to: " << outputPath.string() << '\n';
    } catch (const std::exception& ex) {
        std::cerr << "Failed to export debug game history: " << ex.what() << '\n';
    }
}