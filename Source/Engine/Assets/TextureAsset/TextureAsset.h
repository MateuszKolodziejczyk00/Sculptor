#pragma once

#include "TextureAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "TextureAssetTypes.h"
#include "DDC.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::as
{

class TEXTURE_ASSET_API TextureDataInitializer : public AssetDataInitializer
{
public:

	explicit TextureDataInitializer(const TextureSourceDefinition& source)
		: m_source(source)
	{
	}

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	TextureSourceDefinition m_source;
};


class TEXTURE_ASSET_API TextureAsset : public AssetInstance
{
	ASSET_TYPE_GENERATED_BODY(TextureAsset, AssetInstance)

public:

	using AssetInstance::AssetInstance;

	const lib::SharedPtr<rdr::TextureView>& GetTextureView() const { return m_textureInstance; }

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void OnInitialize() override;
	// End AssetInstance overrides

private:

	Bool CompileTexture();

	void CreateTextureInstance();

	lib::SharedPtr<rdr::TextureView> m_textureInstance;
};

SPT_REGISTER_ASSET_TYPE(TextureAsset);

} // spt::as
