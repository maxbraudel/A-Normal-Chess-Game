#pragma once
#include <vector>
#include <SFML/System/Vector2.hpp>
#include "Kingdom/KingdomId.hpp"
#include "Units/Piece.hpp"
#include "Buildings/Building.hpp"

class Kingdom {
public:
    KingdomId id;
    int gold;
    std::vector<Piece> pieces;
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
    Building* getBuildingAt(sf::Vector2i pos);

    void addPiece(const Piece& piece);
    void removePiece(int pieceId);
    void addBuilding(const Building& building);
    void removeBuilding(int buildingId);
};
