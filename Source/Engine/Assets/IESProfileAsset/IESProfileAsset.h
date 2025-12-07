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
	ASSET_TYPE_GENERATED_BODY(IESProfileAsset, AssetInstance)

public:

	using AssetInstance::AssetInstance;


	lib::SharedPtr<rdr::TextureView> GetTextureView() const { return m_texture; }

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void PostInitialize() override;
	// End AssetInstance overrides

private:

	Bool CompileIESProfileTexture();

	void InitGPUTexture();

	lib::SharedPtr<rdr::TextureView> m_texture;

};
SPT_REGISTER_ASSET_TYPE(IESProfileAsset);

} // spt::as
