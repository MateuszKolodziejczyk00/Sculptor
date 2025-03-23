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


struct AssetInitializer
{
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
	}

	~AssetInstance();

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

	friend srl::TypeSerializer<AssetInstance>;
};


using AssetHandle = lib::MTHandle<AssetInstance>;

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
			serializer.Serialize("Blackboard", static_cast<lib::Blackboard&>(data.blackboard));
		}
		else
		{
			serializer.Serialize("Blackboard", static_cast<const lib::Blackboard&>(data.blackboard));
		}
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::as::AssetInstanceData);


#define SPT_REGISTER_ASSET_DATA_TYPE(DataType) \
	SPT_REGISTER_TYPE_FOR_BLACKBOARD_SERIALIZATION(DataType)

