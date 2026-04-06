#include "Units/PieceFactory.hpp"

PieceFactory::PieceFactory() : m_nextId(0) {}

Piece PieceFactory::createPiece(PieceType type, KingdomId kingdom, sf::Vector2i pos) {
    return Piece(m_nextId++, type, kingdom, pos);
}
