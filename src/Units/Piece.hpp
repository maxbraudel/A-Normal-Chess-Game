#pragma once
#include <SFML/System/Vector2.hpp>
#include "Units/PieceType.hpp"
#include "Kingdom/KingdomId.hpp"

class GameConfig;

class Piece {
public:
    int id;
    PieceType type;
    KingdomId kingdom;
    sf::Vector2i position;
    int xp;
    int formationId;

    Piece();
    Piece(int id, PieceType type, KingdomId kingdom, sf::Vector2i pos);

    bool canUpgradeTo(PieceType target, const GameConfig& config) const;
    int getLevel() const;
};
