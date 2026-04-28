#pragma once
#include <optional>
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
    std::optional<sf::Vector2i> wallBreachEntryDelta;
    std::optional<sf::Vector2i> wallBreachCell;

    Piece();
    Piece(int id, PieceType type, KingdomId kingdom, sf::Vector2i pos);

    bool canUpgradeTo(PieceType target, const GameConfig& config) const;
    int getLevel() const;
    bool hasWallBreachEntryStateFor(sf::Vector2i occupiedCell) const;
    void setWallBreachEntryState(sf::Vector2i entryDelta, sf::Vector2i wallCell);
    void clearWallBreachEntryState();
};
