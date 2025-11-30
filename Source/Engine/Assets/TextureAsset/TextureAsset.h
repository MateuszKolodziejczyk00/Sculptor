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

struct CompiledTextureData
{
	CompiledTexture texture;

	DerivedDataKey derivedDataKey;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Texture",        texture);
		serializer.Serialize("DerivedDataKey", derivedDataKey);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(CompiledTextureData);


struct RuntimeTexture
{
	lib::SharedPtr<rdr::TextureView> textureInstance;
};


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
protected:

	using Super = AssetInstance;

public:

	using AssetInstance::AssetInstance;

	// Begin AssetInstance overrides
	virtual void PostCreate() override;
	virtual void PostInitialize() override;

	static void  OnAssetDeleted(AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data);
	// End AssetInstance overrides

	const lib::SharedPtr<rdr::TextureView>& GetTextureView() const
	{
		return m_cachedRuntimeTexture->textureInstance;
	}

private:

	void CompileTexture();

	void CreateTextureInstance();

	RuntimeTexture* m_cachedRuntimeTexture = nullptr;
};

SPT_REGISTER_ASSET_TYPE(TextureAsset);

} // spt::as
