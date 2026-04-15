#include "Units/PieceFactory.hpp"

PieceFactory::PieceFactory() : m_nextId(0) {}

Piece PieceFactory::createPiece(PieceType type, KingdomId kingdom, sf::Vector2i pos) {
    return Piece(m_nextId++, type, kingdom, pos);
}

void PieceFactory::reset(int nextId) {
    m_nextId = nextId;
}

void PieceFactory::observeExisting(int existingId) {
    if (existingId >= m_nextId) {
        m_nextId = existingId + 1;
    }
}
