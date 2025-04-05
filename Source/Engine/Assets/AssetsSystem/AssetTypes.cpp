#include "AssetTypes.h"
#include "AssetsSystem.h"


namespace spt::as
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// AssetFactory ==================================================================================

AssetFactory& AssetFactory::GetInstance()
{
	static AssetFactory instance;
	return instance;
}

AssetHandle AssetFactory::CreateAsset(AssetsSystem& owningSystem, const AssetInitializer& initializer)
{
	const auto it = m_assetTypes.find(initializer.type);
	if (it == m_assetTypes.end())
	{
		return nullptr;
	}

	const auto& factory = it->second.factory;
	return factory(owningSystem, initializer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// AssetInstance =================================================================================

AssetInstance::~AssetInstance()
{
	m_owningSystem.OnAssetUnloaded(*this);
}

void AssetInstance::SaveAsset()
{
	m_owningSystem.SaveAsset(AssetHandle(this));
}

} // spt::as