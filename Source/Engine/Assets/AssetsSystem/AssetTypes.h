#pragma once

#include "SculptorCoreTypes.h"
#include "Blackboard.h"
#include "BlackboardSerialization.h"
#include "FileSystem/File.h"
#include "ResourcePath.h"
#include "Utility/Threading/ThreadUtils.h"
#include "Serialization.h"


namespace spt::as
{

class AssetsSystem;
class AssetInstance;


using AssetType = lib::HashedString;


template<typename TAssetType>
static AssetType CreateAssetType()
{
	return AssetType(lib::TypeInfo<TAssetType>().name);
}


class ASSETS_SYSTEM_API AssetDataInitializer
{
public:

	virtual void InitializeNewAsset(AssetInstance& asset) = 0;
};


struct AssetInitializer
{
	AssetType             type;
	ResourcePath          path;
	AssetDataInitializer* dataInitializer = nullptr;
};


class AssetBlackboard : public lib::Blackboard
{
public:

	using Super = lib::Blackboard;

	template<typename TType>
	void Unload()
	{
		Unload(lib::TypeInfo<TType>());
	}

	template<typename TType>
	void Remove()
	{
		Remove(lib::TypeInfo<TType>());
	}

	void Unload(lib::RuntimeTypeInfo type)
	{
		m_unloadedTypes.emplace(type);
		Super::Remove(type);
	}

	void Remove(lib::RuntimeTypeInfo type)
	{
		m_unloadedTypes.erase(type);
		Super::Remove(type);
	}

	const lib::HashSet<lib::RuntimeTypeInfo>& GetUnloadedTypes() const { return m_unloadedTypes; }

private:

	lib::HashSet<lib::RuntimeTypeInfo> m_unloadedTypes;
};


struct AssetInstanceData
{
	AssetType       type;
	AssetBlackboard blackboard;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Type", type);
		serializer.Serialize("Blackboard", blackboard);
	}
};


class ASSETS_SYSTEM_API AssetInstance : public lib::MTRefCounted
{
public:

	explicit AssetInstance(AssetsSystem& owningSystem, AssetInitializer initializer)
		: m_name(std::move(initializer.path.GetName()))
		, m_path(std::move(initializer.path))
		, m_owningSystem(owningSystem)
	{
		SPT_CHECK_MSG(initializer.type.IsValid(), "Invalid asset type");

		m_data.type = initializer.type;
	}

	virtual ~AssetInstance();

	// Called after the asset is created and its data is initialized by intializer
	virtual void PostCreate() {}

	// Called after asset's data is loaded
	virtual void PostLoad()   {}

	// Called after asset is ready to be used (regardless of whether it was loaded or created)
	virtual void PostInitialize() {}

	virtual void PreSave() {}
	virtual void PostSave() {}

	virtual void PreUnload() {}

	// This can be overridden by child class. In order to do that, create the same static function with the same signature
	static void OnAssetDeleted(AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data) {}

	void                     AssignData(AssetInstanceData data) { m_data = std::move(data); }
	const AssetInstanceData& GetInstanceData() const            { return m_data; }

	void SaveAsset();

	const lib::HashedString& GetName() const { return m_name; }
	const lib::Path&         GetPath() const { return m_path.GetPath(); }

	const AssetBlackboard& GetBlackboard() const { return m_data.blackboard; }
	AssetBlackboard&       GetBlackboard() { return m_data.blackboard; }

	AssetsSystem& GetOwningSystem() const { return m_owningSystem; }

private:

	lib::HashedString m_name;
	ResourcePath      m_path;

	AssetsSystem& m_owningSystem;

	AssetInstanceData m_data;
};


using AssetHandle = lib::MTHandle<AssetInstance>;


class ASSETS_SYSTEM_API AssetFactory
{
public:

	static AssetFactory& GetInstance();

	template<typename TAssetType>
	void RegisterAssetType()
	{
		SPT_STATIC_CHECK_MSG(std::is_base_of_v<AssetInstance, TAssetType>, "TAssetType must be derived from AssetInstance");

		const auto factory = [](AssetsSystem& owningSystem, AssetInitializer initializer) -> AssetHandle
		{
			return new TAssetType(owningSystem, std::move(initializer));
		};

		const auto deleter = [](AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data) -> void
		{
			TAssetType::OnAssetDeleted(assetSystem, path, data);
		};

		m_assetTypes[CreateAssetType<TAssetType>()] = AssetTypeMetaData{ .factory = factory, .deleter = deleter };
	}

	AssetHandle CreateAsset(AssetsSystem& owningSystem, const AssetInitializer& initializer);

	void DeleteAsset(AssetType assetType, AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data);

private:

	AssetFactory() = default;

	struct AssetTypeMetaData
	{
		lib::RawCallable<AssetHandle(AssetsSystem& owningSystem, AssetInitializer initializer)>                    factory;
		lib::RawCallable<void(AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data)> deleter;
	};

	lib::HashMap<AssetType, AssetTypeMetaData> m_assetTypes;
};


template<typename TAssetType>
struct AssetTypeRegistration
{
	AssetTypeRegistration()
	{
		AssetFactory::GetInstance().RegisterAssetType<TAssetType>();
	}
};

} // spt::as

#define SPT_REGISTER_ASSET_DATA_TYPE(DataType) \
	SPT_REGISTER_TYPE_FOR_BLACKBOARD_SERIALIZATION(DataType)

#define SPT_REGISTER_ASSET_TYPE(AssetType) \
	inline spt::as::AssetTypeRegistration<AssetType> g_assetTypeRegistration_##AssetType; \
	using AssetType##Handle = lib::MTHandle<AssetType>;
