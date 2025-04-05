#include "AssetTypes.h"
#include "AssetsSystem.h"


namespace spt::as
{

AssetInstance::~AssetInstance()
{
	m_owningSystem.OnAssetUnloaded(*this);
}

void AssetInstance::SaveAsset()
{
	m_owningSystem.SaveAsset(AssetHandle(this));
}

} // spt::as