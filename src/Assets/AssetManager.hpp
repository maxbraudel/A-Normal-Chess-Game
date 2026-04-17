#pragma once
#include <map>
#include <string>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Font.hpp>
#include "Board/CellType.hpp"
#include "Buildings/BuildingType.hpp"
#include "Objects/MapObjectType.hpp"
#include "Units/PieceType.hpp"
#include "Kingdom/KingdomId.hpp"

class AssetManager {
public:
    AssetManager();
    void loadAll(const std::string& assetsDir);

    const sf::Texture& getCellTexture(CellType type) const;
    const sf::Texture& getBuildingTexture(BuildingType type) const;
    const sf::Texture& getBuildingTexture(BuildingType type, int localX, int localY) const;
    const sf::Texture& getPieceTexture(PieceType type, KingdomId kingdom) const;
    const sf::Texture& getInfernalPieceTexture(PieceType type) const;
    const sf::Texture& getMapObjectTexture(MapObjectType type) const;
    const sf::Texture& getUITexture(const std::string& name) const;
    const sf::Font& getFont() const;
    bool hasFont() const;

private:
    std::map<std::string, sf::Texture> m_textures;
    sf::Texture m_fallback;
    sf::Font m_font;
    bool m_hasFont;

    void loadTexture(const std::string& key, const std::string& path);
    std::string pieceKey(PieceType type, KingdomId kingdom) const;
    std::string infernalPieceKey(PieceType type) const;
    std::string cellKey(CellType type) const;
    std::string buildingKey(BuildingType type) const;
    std::string buildingChunkKey(BuildingType type, int localX, int localY) const;
    std::string mapObjectKey(MapObjectType type) const;
};
