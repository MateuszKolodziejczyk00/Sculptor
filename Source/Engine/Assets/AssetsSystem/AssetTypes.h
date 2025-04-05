#pragma once

#include "SculptorCoreTypes.h"
#include "Blackboard.h"
#include "BlackboardSerialization.h"
#include "FileSystem/File.h"
#include "ResourcePath.h"
#include "Utility/Threading/ThreadUtils.h"


namespace spt::as
{

class AssetsSystem;


using AssetType = lib::HashedString;


template<typename TAssetType>
static AssetType CreateAssetType()
{
	return AssetType(lib::TypeInfo<TAssetType>().name);
}


struct AssetInitializer
{
	AssetType    type;
	ResourcePath path;
};


class AssetBlackboard : public lib::Blackboard
{
public:

	using Super = lib::Blackboard;

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
};



struct ASSETS_SYSTEM_API AssetInstance : public lib::MTRefCounted
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

	void                     AssignData(AssetInstanceData data) { m_data = std::move(data); }
	const AssetInstanceData& GetInstanceData() const            { return m_data; }

	void SaveAsset();

	const lib::HashedString& GetName() const { return m_name; }
	const lib::Path&         GetPath() const { return m_path.GetPath(); }

	const AssetBlackboard& GetBlackboard() const { return m_data.blackboard; }
	AssetBlackboard&       GetBlackboard() { return m_data.blackboard; }

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

		m_assetTypes[CreateAssetType<TAssetType>()] = AssetTypeMetaData{ .factory = factory };
	}

	AssetHandle CreateAsset(AssetsSystem& owningSystem, const AssetInitializer& initializer);

private:

	AssetFactory() = default;

	struct AssetTypeMetaData
	{
		lib::RawCallable<AssetHandle(AssetsSystem& owningSystem, AssetInitializer initializer)> factory;
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

namespace spt::srl
{

template<>
struct TypeSerializer<as::AssetInstanceData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{

		if constexpr (Serializer::IsLoading())
		{
			lib::String typeName;
			serializer.Serialize("Type", typeName);
			data.type = typeName;

			serializer.Serialize("Blackboard", static_cast<lib::Blackboard&>(data.blackboard));
		}
		else
		{
			serializer.Serialize("Blackboard", static_cast<const lib::Blackboard&>(data.blackboard));
			serializer.Serialize("Type", data.type.GetView());
		}
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::as::AssetInstanceData);


#define SPT_REGISTER_ASSET_DATA_TYPE(DataType) \
	SPT_REGISTER_TYPE_FOR_BLACKBOARD_SERIALIZATION(DataType)

#define SPT_REGISTER_ASSET_TYPE(AssetType) \
	spt::as::AssetTypeRegistration<AssetType> g_assetTypeRegistration_##AssetType;

