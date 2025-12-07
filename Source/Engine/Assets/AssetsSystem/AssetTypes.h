#pragma once

#include "SculptorCoreTypes.h"
#include "Blackboard.h"
#include "BlackboardSerialization.h"
#include "FileSystem/File.h"
#include "ResourcePath.h"
#include "Utility/Threading/ThreadUtils.h"
#include "Serialization.h"
#include "DDC.h"

#define SPT_REGISTER_ASSET_DATA_TYPE(DataType) \
	SPT_REGISTER_TYPE_FOR_BLACKBOARD_SERIALIZATION(DataType)

namespace spt::as
{

class AssetsSystem;
class AssetInstance;


using AssetType    = lib::HashedString;
using AssetTypeKey = SizeType;


template<typename TAssetType>
static AssetType CreateAssetType()
{
	return AssetType(lib::TypeInfo<TAssetType>().name);
}


template<typename TAssetType>
static AssetTypeKey CreateAssetTypeKey()
{
	return CreateAssetType<TAssetType>().GetKey();
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


using AssetBlackboard = lib::Blackboard;


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


struct AssetDerivedDataKey
{
	AssetDerivedDataKey(const ResourcePath& path, lib::HashedString subData)
		: ddcKey(path.GetID(), subData.GetKey())
	{ }

	AssetDerivedDataKey(const ResourcePath& path)
		: ddcKey(path.GetID(), 0u)
	{ }

	operator DerivedDataKey() const { return ddcKey; }

	DerivedDataKey ddcKey;
};


template<typename THeader>
struct DDCLoadedData : public lib::MTRefCounted
{
	DDCResourceHandle     handle;
	THeader               header;
	lib::Span<const Byte> bin;
};


#define ASSET_TYPE_GENERATED_BODY(type, parent) \
protected: \
	using Super = parent; \
	friend class AssetFactory; \
private: \


enum class EAssetRuntimeFlags : Uint32
{
	None,
	Compiled = BIT(0),

	Default = None
};


class ASSETS_SYSTEM_API AssetInstance : public lib::MTRefCounted
{
	friend AssetsSystem;

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

	Bool IsCompiled() const { return lib::HasAnyFlag(m_runtimeFlags, EAssetRuntimeFlags::Compiled); }

	void                     AssignData(AssetInstanceData data) { m_data = std::move(data); }
	const AssetInstanceData& GetInstanceData() const            { return m_data; }

	void SaveAsset();

	const lib::HashedString& GetName() const { return m_name; }
	const lib::Path&         GetRelativePath() const { return m_path.GetPath(); }
	const AssetType&         GetType() const { return m_data.type; }
	AssetTypeKey             GetTypeKey() const { return m_data.type.GetKey(); }
	const lib::Path          GetDirectoryPath() const;

	const AssetBlackboard& GetBlackboard() const { return m_data.blackboard; }
	AssetBlackboard&       GetBlackboard() { return m_data.blackboard; }

	AssetsSystem& GetOwningSystem() const { return m_owningSystem; }

protected:

	// Called after the asset is created and its data is initialized by initializer
	virtual void PostCreate() {}

	virtual Bool Compile() = 0;

	// Called after asset is ready to be used (regardless of whether it was loaded or created)
	virtual void PostInitialize() {}

	virtual void PreSave() {}
	virtual void PostSave() {}

	virtual void PreUnload() {}

	void AddRuntimeFlag(EAssetRuntimeFlags flag) { lib::AddFlag(m_runtimeFlags, flag); }

	// This can be overridden by child class. In order to do that, create the same static function with the same signature
	static void OnAssetDeleted(AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data) {}

	operator AssetDerivedDataKey() const { return AssetDerivedDataKey(GetRelativePath()); }

	template<typename THeader>
	void CreateDerivedData(AssetDerivedDataKey key, const THeader& header, lib::Span<const Byte> data);

	template<typename THeader>
	lib::MTHandle<DDCLoadedData<THeader>> LoadDerivedData(AssetDerivedDataKey key);

private:

	lib::HashedString m_name;
	ResourcePath      m_path;

	EAssetRuntimeFlags m_runtimeFlags = EAssetRuntimeFlags::Default;

	AssetsSystem& m_owningSystem;

	AssetInstanceData m_data;
};


template<typename THeader>
void AssetInstance::CreateDerivedData(AssetDerivedDataKey key, const THeader& header, lib::Span<const Byte> data)
{
	srl::Serializer serializer = srl::Serializer::CreateWriter();
	const_cast<THeader&>(header).Serialize(serializer);
	const lib::String headerData = serializer.ToCompactString();

	const Uint32 headerSize = static_cast<Uint32>(headerData.size());
	const Uint32 totalSize = sizeof(Uint32) + headerSize + static_cast<Uint32>(data.size());

	DDCResourceHandle handle = GetOwningSystem().GetDDC().CreateDerivedData(key, totalSize);
	const lib::Span<Byte> mutableSpan = handle.GetMutableSpan();

	std::memcpy(mutableSpan.data(), &headerSize, sizeof(Uint32));
	std::memcpy(mutableSpan.data() + sizeof(Uint32), headerData.data(), headerSize);
	std::memcpy(mutableSpan.data() + sizeof(Uint32) + headerSize, data.data(), data.size());
}


template<typename THeader>
lib::MTHandle<DDCLoadedData<THeader>> AssetInstance::LoadDerivedData(AssetDerivedDataKey key)
{
	DDCResourceHandle ddcHandle = GetOwningSystem().GetDDC().GetResourceHandle(key);

	if (!ddcHandle.IsValid())
	{
		return nullptr;
	}

	lib::MTHandle<DDCLoadedData<THeader>> result = new DDCLoadedData<THeader>();
	result->handle = std::move(ddcHandle);

	const lib::Span<const Byte> immutableSpan = result->handle.GetImmutableSpan();

	Uint32 headerSize = 0;
	std::memcpy(&headerSize, immutableSpan.data(), sizeof(Uint32));
	const lib::StringView headerData(reinterpret_cast<const char*>(immutableSpan.data() + sizeof(Uint32)), headerSize);
	srl::Serializer serializer = srl::Serializer::CreateReader(headerData);
	result->header.Serialize(serializer);

	const Uint32 dataSize = static_cast<Uint32>(immutableSpan.size()) - sizeof(Uint32) - headerSize;
	result->bin = lib::Span<const Byte>(immutableSpan.data() + sizeof(Uint32) + headerSize, dataSize);

	return result;
}


using AssetHandle = lib::MTHandle<AssetInstance>;

template<typename TAssetType>
using TypedAssetHandle = lib::MTHandle<TAssetType>;


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

		m_assetTypes[CreateAssetTypeKey<TAssetType>()] = AssetTypeMetaData{ .type = CreateAssetType<TAssetType>(), .factory = factory, .deleter = deleter};
	}

	AssetHandle CreateAsset(AssetsSystem& owningSystem, const AssetInitializer& initializer);

	void DeleteAsset(AssetType assetType, AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data);

	AssetType GetAssetTypeByKey(AssetTypeKey key) const;

private:

	AssetFactory() = default;

	struct AssetTypeMetaData
	{
		AssetType type;
		lib::RawCallable<AssetHandle(AssetsSystem& owningSystem, AssetInitializer initializer)>                    factory;
		lib::RawCallable<void(AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data)> deleter;
	};

	lib::HashMap<AssetTypeKey, AssetTypeMetaData> m_assetTypes;
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

#define SPT_REGISTER_ASSET_TYPE(AssetType) \
	inline spt::as::AssetTypeRegistration<AssetType> g_assetTypeRegistration_##AssetType; \
	using AssetType##Handle = lib::MTHandle<AssetType>;
