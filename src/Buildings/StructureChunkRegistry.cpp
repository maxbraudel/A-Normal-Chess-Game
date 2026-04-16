#include "Buildings/StructureChunkRegistry.hpp"

namespace {

const std::vector<StructureChunkDefinition> kStructureChunkDefinitions = {
    {BuildingType::Barracks, "barracks", 4, 3},
    {BuildingType::Church, "church", 4, 3},
    {BuildingType::Mine, "mine", 6, 6},
    {BuildingType::Farm, "farm", 6, 4},
    {BuildingType::Arena, "arena", 4, 4},
};

}

const std::vector<StructureChunkDefinition>& StructureChunkRegistry::all() {
    return kStructureChunkDefinitions;
}

const StructureChunkDefinition* StructureChunkRegistry::find(BuildingType type) {
    for (const auto& definition : kStructureChunkDefinitions) {
        if (definition.type == type) {
            return &definition;
        }
    }

    return nullptr;
}

bool StructureChunkRegistry::hasChunkedTextures(BuildingType type) {
    return find(type) != nullptr;
}

bool StructureChunkRegistry::hasChunkAt(BuildingType type, int localX, int localY) {
    const StructureChunkDefinition* definition = find(type);
    if (!definition) {
        return false;
    }

    return localX >= 0 && localX < definition->width
        && localY >= 0 && localY < definition->height;
}

int StructureChunkRegistry::getChunkWidth(BuildingType type, int fallbackWidth) {
    const StructureChunkDefinition* definition = find(type);
    return definition ? definition->width : fallbackWidth;
}

int StructureChunkRegistry::getChunkHeight(BuildingType type, int fallbackHeight) {
    const StructureChunkDefinition* definition = find(type);
    return definition ? definition->height : fallbackHeight;
}

std::string StructureChunkRegistry::makeChunkTextureKey(BuildingType type, int localX, int localY) {
    const StructureChunkDefinition* definition = find(type);
    if (!definition || !hasChunkAt(type, localX, localY)) {
        return "";
    }

    return std::string("building_chunk_") + definition->id + "_"
        + std::to_string(localX + 1) + "_" + std::to_string(localY + 1);
}

std::string StructureChunkRegistry::makeChunkTextureRelativePath(BuildingType type, int localX, int localY) {
    const StructureChunkDefinition* definition = find(type);
    if (!definition || !hasChunkAt(type, localX, localY)) {
        return "";
    }

    return std::string("/textures/cells/structures/") + definition->id + "/" + definition->id + "_"
        + std::to_string(localX + 1) + "_" + std::to_string(localY + 1) + ".png";
}