#include "Units/Piece.hpp"
#include "Config/GameConfig.hpp"

Piece::Piece() : id(-1), type(PieceType::Pawn), kingdom(KingdomId::White),
                 position(0, 0), xp(0), formationId(-1) {}

Piece::Piece(int id, PieceType type, KingdomId kingdom, sf::Vector2i pos)
    : id(id), type(type), kingdom(kingdom), position(pos), xp(0), formationId(-1) {}

bool Piece::canUpgradeTo(PieceType target, const GameConfig& config) const {
    if (type == PieceType::Pawn) {
        if (target == PieceType::Knight || target == PieceType::Bishop) {
            return xp >= config.getXPThresholdPawnToKnightOrBishop();
        }
    }
    if (type == PieceType::Knight || type == PieceType::Bishop) {
        if (target == PieceType::Rook) {
            return xp >= config.getXPThresholdToRook();
        }
    }
    return false;
}

int Piece::getLevel() const {
    switch (type) {
        case PieceType::Pawn: return 0;
        case PieceType::Knight: return 1;
        case PieceType::Bishop: return 1;
        case PieceType::Rook: return 2;
        case PieceType::Queen: return 3;
        case PieceType::King: return 4;
    }
    return 0;
}
