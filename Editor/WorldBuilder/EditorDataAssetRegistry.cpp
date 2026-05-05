#include "stdafx.h"
#include "EditorDataAssetRegistry.h"

EditorDataAssetRegistry& EditorDataAssetRegistry::GetInstance()
{
    static EditorDataAssetRegistry instance;
    return instance;
}

void EditorDataAssetRegistry::RegisterAsset(const std::string& assetType, const std::string& assetName, const std::string& filePath)
{
    // Load general data assets as raw text buffers or custom structural resources mapped internally by Wicked
    wi::Resource res = wi::resourcemanager::Load(filePath, wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA);
    
    if (res.IsValid())
    {
        m_registry[assetType][assetName] = res;
    }
    else
    {
        wi::backlog::post("[EditorDataAssetRegistry] Failed to register resource: " + filePath);
    }
}

wi::Resource EditorDataAssetRegistry::GetAsset(const std::string& assetType, const std::string& assetName)
{
    auto typeIt = m_registry.find(assetType);
    if (typeIt != m_registry.end())
    {
        auto nameIt = typeIt->second.find(assetName);
        if (nameIt != typeIt->second.end())
        {
            return nameIt->second;
        }
    }
    return wi::Resource(); // Returns invalid default resource
}
