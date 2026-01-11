#pragma once

#include "MaterialAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "ECSRegistry.h"
#include "MaterialInstance/MaterialInstance.h"


namespace spt::as
{

struct MaterialTypeInfo
{
	lib::RuntimeTypeInfo materialType;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("MaterialType", materialType);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(MaterialTypeInfo);



class MATERIAL_ASSET_API MaterialAsset : public AssetInstance
{
	ASSET_TYPE_GENERATED_BODY(MaterialAsset, AssetInstance)

public:

	using AssetInstance::AssetInstance;

	ecs::EntityHandle GetMaterialEntity() const;

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void OnInitialize() override;
	// End AssetInstance overrides

private:

	lib::UniquePtr<MaterialInstance> m_materialInstance;
};
SPT_REGISTER_ASSET_TYPE(MaterialAsset);

} // spt::as
