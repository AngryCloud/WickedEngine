#pragma once

#include "WickedEngine.h"
#include <string>
#include <unordered_map>

// ------------------------------------------------------------------
// Editor Data Asset Registry (central store for all PCG/biome tools)
// Hooks directly into Wicked Engine Resource Manager.
// ------------------------------------------------------------------
class EditorDataAssetRegistry
{
public:
    static EditorDataAssetRegistry& GetInstance();

    // Register a Data Asset (biome, cave module set, forest rule set)
    // using the Wicked Engine resource manager
    void RegisterAsset(const std::string& assetType, const std::string& assetName, const std::string& filePath);
    
    // Retrieve the active resource handler
    wi::Resource GetAsset(const std::string& assetType, const std::string& assetName);

private:
    std::unordered_map<std::string, std::unordered_map<std::string, wi::Resource>> m_registry;
};
