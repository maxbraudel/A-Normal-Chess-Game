#pragma once
#include <optional>
#include <vector>
#include <deque>
#include <SFML/System/Vector2.hpp>
#include "Kingdom/KingdomId.hpp"
#include "Units/Piece.hpp"
#include "Buildings/Building.hpp"

class Kingdom {
public:
    KingdomId id;
    int gold;
    bool hasSpawnedBishop = false;
    int lastBishopSpawnParity = 0;
    std::deque<Piece> pieces;
    std::vector<Building> buildings;

    Kingdom();
    Kingdom(KingdomId id);

    Piece* getKing();
    const Piece* getKing() const;
    bool hasQueen() const;
    bool hasSubjects() const;
    int pieceCount() const;
    Piece* getPieceAt(sf::Vector2i pos);
    const Piece* getPieceAt(sf::Vector2i pos) const;
    Piece* getPieceById(int pieceId);
    const Piece* getPieceById(int pieceId) const;
    Building* getBuildingAt(sf::Vector2i pos);
    std::optional<int> preferredNextBishopSpawnParity() const;
    void recordSuccessfulBishopSpawnParity(int parity);

    void addPiece(const Piece& piece);
    void removePiece(int pieceId);
    void addBuilding(const Building& building);
    void removeBuilding(int buildingId);
};
