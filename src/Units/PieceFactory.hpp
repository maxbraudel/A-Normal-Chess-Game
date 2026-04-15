#pragma once
#include "Units/Piece.hpp"
#include <SFML/System/Vector2.hpp>

class PieceFactory {
public:
    PieceFactory();
    Piece createPiece(PieceType type, KingdomId kingdom, sf::Vector2i pos);
    void reset(int nextId = 0);
    void observeExisting(int existingId);
private:
    int m_nextId;
};
