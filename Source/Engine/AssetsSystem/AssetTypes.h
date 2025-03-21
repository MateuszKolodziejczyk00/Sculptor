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


struct AssetInstanceData
{
	lib::Blackboard blackboard;
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

	const lib::Blackboard& GetBlackboard() const { return m_data.blackboard; }
	lib::Blackboard&       GetBlackboard() { return m_data.blackboard; }

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
		serializer.Serialize("Blackboard", data.blackboard);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::as::AssetInstanceData);


#define SPT_REGISTER_ASSET_DATA_TYPE(DataType) \
	SPT_REGISTER_TYPE_FOR_BLACKBOARD_SERIALIZATION(DataType)

