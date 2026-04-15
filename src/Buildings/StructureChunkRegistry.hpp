#pragma once

#include <string>
#include <vector>

#include "Buildings/BuildingType.hpp"

struct StructureChunkDefinition {
    BuildingType type;
    const char* id;
    int width;
    int height;
};

class StructureChunkRegistry {
public:
    static const std::vector<StructureChunkDefinition>& all();
    static const StructureChunkDefinition* find(BuildingType type);
    static bool hasChunkedTextures(BuildingType type);
    static bool hasChunkAt(BuildingType type, int localX, int localY);
    static int getChunkWidth(BuildingType type, int fallbackWidth);
    static int getChunkHeight(BuildingType type, int fallbackHeight);
    static std::string makeChunkTextureKey(BuildingType type, int localX, int localY);
    static std::string makeChunkTextureRelativePath(BuildingType type, int localX, int localY);
};