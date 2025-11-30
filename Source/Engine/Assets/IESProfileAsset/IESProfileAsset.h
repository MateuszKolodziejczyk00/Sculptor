#pragma once

#include "IESProfileAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "DDC.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::as
{

struct IESProfileSourceDefinition
{
	lib::Path path;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Path", path);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(IESProfileSourceDefinition);

struct CompiledIESProfile
{
	math::Vector2u resolution = math::Vector2u(0u, 0u);
	Real32         lightSourceCandela = 1.f;
	DerivedDataKey derivedDataKey;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Resolution", resolution);
		serializer.Serialize("LightSourceCandela", lightSourceCandela);
		serializer.Serialize("DerivedDataKey", derivedDataKey);
	}
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
