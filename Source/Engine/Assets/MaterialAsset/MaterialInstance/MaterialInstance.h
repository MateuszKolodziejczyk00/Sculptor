#pragma once

#include "MaterialAssetMacros.h"
#include "AssetTypes.h"
#include "SculptorCoreTypes.h"
#include "ECSRegistry.h"
#include "Utility/Templates/Callable.h"


namespace spt::as
{

class MATERIAL_ASSET_API MaterialInstance
{
public:

	virtual ~MaterialInstance() = default;

	virtual lib::DynamicArray<Byte> Compile(const AssetInstance& asset) = 0;
	virtual void Load(const AssetInstance& asset, lib::MTHandle<DDCLoadedBin> loadedBlob) = 0;

	ecs::EntityHandle GetMaterialEntity() const { return m_materialEntity; }

protected:

	ecs::EntityHandle m_materialEntity;
};


using MaterialInstanceFactory = lib::RawCallable<lib::UniquePtr<MaterialInstance>()>;


class MATERIAL_ASSET_API MaterialInstanceTypesRegistry
{
public:

	static void RegisterMaterialInstanceType(const lib::RuntimeTypeInfo& type, MaterialInstanceFactory factory);

	static lib::UniquePtr<MaterialInstance> CreateMaterialInstance(const lib::RuntimeTypeInfo& type);

private:

	static MaterialInstanceTypesRegistry& GetInstance();

	MaterialInstanceTypesRegistry() = default;

	lib::HashMap<lib::RuntimeTypeInfo, MaterialInstanceFactory> m_factories;
};


template<typename TInstanceType>
struct MaterialInstanceTypeRegistrator
{
	MaterialInstanceTypeRegistrator()
	{
		MaterialInstanceTypesRegistry::RegisterMaterialInstanceType(lib::TypeInfo<TInstanceType>(), MaterialInstanceFactory([]() { return lib::UniquePtr<MaterialInstance>(new TInstanceType()); }));
	}
};


#define SPT_REGISTER_MATERIAL_INSTANCE_TYPE(type) \
static spt::as::MaterialInstanceTypeRegistrator<type> SPT_SCOPE_NAME_EVAL(autoRegistrator, __LINE__);

} // spt::as
