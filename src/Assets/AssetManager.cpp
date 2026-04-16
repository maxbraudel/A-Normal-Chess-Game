#include "Assets/AssetManager.hpp"

#include "Buildings/StructureChunkRegistry.hpp"

#include <iostream>

AssetManager::AssetManager() : m_hasFont(false) {
    // Create a small fallback texture (magenta)
    sf::Image img;
    img.create(16, 16, sf::Color::Magenta);
    m_fallback.loadFromImage(img);
}

void AssetManager::loadTexture(const std::string& key, const std::string& path) {
    sf::Texture tex;
    if (tex.loadFromFile(path)) {
        m_textures[key] = std::move(tex);
    } else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
}

void AssetManager::loadAll(const std::string& assetsDir) {
    // Cell textures
    loadTexture("cell_grass", assetsDir + "/textures/cells/grass.png");
    loadTexture("cell_dirt", assetsDir + "/textures/cells/dirt.png");
    loadTexture("cell_water", assetsDir + "/textures/cells/water.png");

    for (const auto& definition : StructureChunkRegistry::all()) {
        for (int localY = 0; localY < definition.height; ++localY) {
            for (int localX = 0; localX < definition.width; ++localX) {
                loadTexture(
                    StructureChunkRegistry::makeChunkTextureKey(definition.type, localX, localY),
                    assetsDir + StructureChunkRegistry::makeChunkTextureRelativePath(definition.type, localX, localY)
                );
            }
        }
    }

    // Building textures
    loadTexture("building_church", assetsDir + "/textures/cells/church.png");
    loadTexture("building_mine", assetsDir + "/textures/cells/mine.png");
    loadTexture("building_farm", assetsDir + "/textures/cells/farm.png");
    loadTexture("building_barracks", assetsDir + "/textures/cells/barrak.png");
    loadTexture("building_woodwall", assetsDir + "/textures/cells/wall_wood.png");
    loadTexture("building_stonewall", assetsDir + "/textures/cells/wall_stone.png");
    loadTexture("building_arena", assetsDir + "/textures/cells/barrak.png"); // reuse barrak for arena
    loadTexture("building_bridge", assetsDir + "/textures/cells/bridge.png");

    // Piece textures - White
    loadTexture("piece_pawn_white", assetsDir + "/textures/pieces/white/pawn.png");
    loadTexture("piece_knight_white", assetsDir + "/textures/pieces/white/knight.png");
    loadTexture("piece_bishop_white", assetsDir + "/textures/pieces/white/bishop.png");
    loadTexture("piece_rook_white", assetsDir + "/textures/pieces/white/rook.png");
    loadTexture("piece_queen_white", assetsDir + "/textures/pieces/white/queen.png");
    loadTexture("piece_king_white", assetsDir + "/textures/pieces/white/king.png");

    // Piece textures - Black
    loadTexture("piece_pawn_black", assetsDir + "/textures/pieces/black/pawn.png");
    loadTexture("piece_knight_black", assetsDir + "/textures/pieces/black/knight.png");
    loadTexture("piece_bishop_black", assetsDir + "/textures/pieces/black/bishop.png");
    loadTexture("piece_rook_black", assetsDir + "/textures/pieces/black/rook.png");
    loadTexture("piece_queen_black", assetsDir + "/textures/pieces/black/queen.png");
    loadTexture("piece_king_black", assetsDir + "/textures/pieces/black/king.png");

    // UI textures
    loadTexture("ui_crossed_swords", assetsDir + "/textures/ui/crossed_swords.png");
    loadTexture("ui_shield_white", assetsDir + "/textures/ui/shield_white.png");
    loadTexture("ui_shield_black", assetsDir + "/textures/ui/shield_black.png");
    loadTexture("ui_build_ongoing", assetsDir + "/textures/ui/build_ongoing.png");
    loadTexture("ui_move_ongoing", assetsDir + "/textures/ui/move_ongoing.png");

    // Font
    if (m_font.loadFromFile(assetsDir + "/fonts/default.ttf")) {
        m_hasFont = true;
    } else {
        std::cerr << "Warning: No font loaded. UI text will not display.\n";
        m_hasFont = false;
    }
}

std::string AssetManager::pieceKey(PieceType type, KingdomId kingdom) const {
    std::string prefix = "piece_";
    switch (type) {
        case PieceType::Pawn: prefix += "pawn"; break;
        case PieceType::Knight: prefix += "knight"; break;
        case PieceType::Bishop: prefix += "bishop"; break;
        case PieceType::Rook: prefix += "rook"; break;
        case PieceType::Queen: prefix += "queen"; break;
        case PieceType::King: prefix += "king"; break;
    }
    prefix += (kingdom == KingdomId::White) ? "_white" : "_black";
    return prefix;
}

std::string AssetManager::cellKey(CellType type) const {
    switch (type) {
        case CellType::Grass: return "cell_grass";
        case CellType::Dirt: return "cell_dirt";
        case CellType::Water: return "cell_water";
        default: return "";
    }
}

std::string AssetManager::buildingKey(BuildingType type) const {
    switch (type) {
        case BuildingType::Church: return "building_church";
        case BuildingType::Mine: return "building_mine";
        case BuildingType::Farm: return "building_farm";
        case BuildingType::Barracks: return "building_barracks";
        case BuildingType::WoodWall: return "building_woodwall";
        case BuildingType::StoneWall: return "building_stonewall";
        case BuildingType::Bridge: return "building_bridge";
        case BuildingType::Arena: return "building_arena";
    }
    return "";
}

std::string AssetManager::buildingChunkKey(BuildingType type, int localX, int localY) const {
    return StructureChunkRegistry::makeChunkTextureKey(type, localX, localY);
}

const sf::Texture& AssetManager::getCellTexture(CellType type) const {
    auto key = cellKey(type);
    auto it = m_textures.find(key);
    if (it != m_textures.end()) return it->second;
    return m_fallback;
}

const sf::Texture& AssetManager::getBuildingTexture(BuildingType type) const {
    auto key = buildingKey(type);
    auto it = m_textures.find(key);
    if (it != m_textures.end()) return it->second;
    return m_fallback;
}

const sf::Texture& AssetManager::getBuildingTexture(BuildingType type, int localX, int localY) const {
    const std::string key = buildingChunkKey(type, localX, localY);
    if (!key.empty()) {
        auto it = m_textures.find(key);
        if (it != m_textures.end()) {
            return it->second;
        }
    }

    return getBuildingTexture(type);
}

const sf::Texture& AssetManager::getPieceTexture(PieceType type, KingdomId kingdom) const {
    auto key = pieceKey(type, kingdom);
    auto it = m_textures.find(key);
    if (it != m_textures.end()) return it->second;
    return m_fallback;
}

const sf::Texture& AssetManager::getUITexture(const std::string& name) const {
    auto key = "ui_" + name;
    auto it = m_textures.find(key);
    if (it != m_textures.end()) return it->second;
    return m_fallback;
}

const sf::Font& AssetManager::getFont() const {
    return m_font;
}

bool AssetManager::hasFont() const {
    return m_hasFont;
}
