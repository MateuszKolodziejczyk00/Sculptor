#pragma once

#include "IESProfileAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "DDC.h"
#include "YAMLSerializerHelper.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::as
{

struct IESProfileSourceDefinition
{
	lib::Path path;
};
SPT_REGISTER_ASSET_DATA_TYPE(IESProfileSourceDefinition);

struct CompiledIESProfile
{
	math::Vector2u resolution = math::Vector2u(0u, 0u);
	Real32         lightSourceCandela = 1.f;
	DerivedDataKey derivedDataKey;
};
SPT_REGISTER_ASSET_DATA_TYPE(CompiledIESProfile);


class IES_PROFILE_ASSET_API IESProfileDataInitializer : public AssetDataInitializer
{
public:

	explicit IESProfileDataInitializer(const IESProfileSourceDefinition& source)
		: m_source(source)
	{
	}

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	IESProfileSourceDefinition m_source;
};


class IES_PROFILE_ASSET_API IESProfileAsset : public AssetInstance
{
protected:

	using Super = AssetInstance;

public:

	using AssetInstance::AssetInstance;

	// Begin AssetInstance overrides
	virtual void PostCreate() override;
	virtual void PostInitialize() override;

	static void  OnAssetDeleted(AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data);
	// End AssetInstance overrides

private:

	void CompileIESProfileTexture();

};
SPT_REGISTER_ASSET_TYPE(IESProfileAsset);

} // spt::as


namespace spt::srl
{

template<>
struct TypeSerializer<as::IESProfileSourceDefinition>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("Path", data.path);
	}
};

template<>
struct TypeSerializer<as::CompiledIESProfile>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("Resolution", data.resolution);
		serializer.Serialize("LightSourceCandela", data.lightSourceCandela);
		serializer.Serialize("DerivedDataKey", data.derivedDataKey);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::as::IESProfileSourceDefinition);
SPT_YAML_SERIALIZATION_TEMPLATES(spt::as::CompiledIESProfile);
