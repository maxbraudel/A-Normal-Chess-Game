#include "Kingdom/Kingdom.hpp"
#include <algorithm>

Kingdom::Kingdom() : id(KingdomId::White), gold(0) {}
Kingdom::Kingdom(KingdomId id) : id(id), gold(0) {}

Piece* Kingdom::getKing() {
    for (auto& p : pieces)
        if (p.type == PieceType::King) return &p;
    return nullptr;
}

const Piece* Kingdom::getKing() const {
    for (const auto& p : pieces)
        if (p.type == PieceType::King) return &p;
    return nullptr;
}

bool Kingdom::hasQueen() const {
    for (const auto& p : pieces)
        if (p.type == PieceType::Queen) return true;
    return false;
}

bool Kingdom::hasSubjects() const {
    int count = 0;
    for (const auto& p : pieces) {
        if (p.type != PieceType::King) ++count;
    }
    return count > 0;
}

int Kingdom::pieceCount() const {
    return static_cast<int>(pieces.size());
}

Piece* Kingdom::getPieceAt(sf::Vector2i pos) {
    for (auto& p : pieces)
        if (p.position == pos) return &p;
    return nullptr;
}

const Piece* Kingdom::getPieceAt(sf::Vector2i pos) const {
    for (const auto& p : pieces)
        if (p.position == pos) return &p;
    return nullptr;
}

Piece* Kingdom::getPieceById(int pieceId) {
    for (auto& p : pieces)
        if (p.id == pieceId) return &p;
    return nullptr;
}

const Piece* Kingdom::getPieceById(int pieceId) const {
    for (const auto& p : pieces)
        if (p.id == pieceId) return &p;
    return nullptr;
}

Building* Kingdom::getBuildingAt(sf::Vector2i pos) {
    for (auto& b : buildings)
        if (b.containsCell(pos.x, pos.y)) return &b;
    return nullptr;
}

std::optional<int> Kingdom::preferredNextBishopSpawnParity() const {
    if (!hasSpawnedBishop) {
        return std::nullopt;
    }

    return 1 - lastBishopSpawnParity;
}

void Kingdom::recordSuccessfulBishopSpawnParity(int parity) {
    hasSpawnedBishop = true;
    lastBishopSpawnParity = parity & 1;
}

void Kingdom::addPiece(const Piece& piece) {
    pieces.push_back(piece);
}

void Kingdom::removePiece(int pieceId) {
    pieces.erase(std::remove_if(pieces.begin(), pieces.end(),
        [pieceId](const Piece& p) { return p.id == pieceId; }), pieces.end());
}

void Kingdom::addBuilding(const Building& building) {
    buildings.push_back(building);
}

void Kingdom::removeBuilding(int buildingId) {
    buildings.erase(std::remove_if(buildings.begin(), buildings.end(),
        [buildingId](const Building& b) { return b.id == buildingId; }), buildings.end());
}
