#pragma once

#include "TextureAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "TextureAssetTypes.h"
#include "StreamableTextureInterface.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::as
{

struct RuntimeTexture
{
	lib::SharedPtr<rdr::TextureView> textureInstance;
};


class TEXTURE_ASSET_API TextureDataInitializer : public AssetDataInitializer
{
public:

	TextureDataInitializer(const TextureDefinition& definition, const TextureData& data)
		: m_definition(definition)
		, m_data(data)
	{
	}

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	TextureDefinition m_definition;
	TextureData       m_data;
};


class TEXTURE_ASSET_API TextureAsset : public AssetInstance, public StreamableTextureInterface
{
protected:

	using Super = AssetInstance;

public:

	using AssetInstance::AssetInstance;

	// Begin AssetInstance overrides
	virtual void PostInitialize() override;
	// End AssetInstance overrides

	// Begin StreamableTextureInterface overrides
	virtual void PrepareForUpload(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& preUploadDependency) override;
	virtual void ScheduleUploads() override;
	virtual void FinishStreaming(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& postUploadDependency) override;
	// End StreamableTextureInterface overrides

	const lib::SharedPtr<rdr::TextureView>& GetTextureView() const
	{
		return m_cachedRuntimeTexture->textureInstance;
	}

private:

	void CreateTextureInstance();

	RuntimeTexture* m_cachedRuntimeTexture = nullptr;
};

SPT_REGISTER_ASSET_TYPE(TextureAsset);

} // spt::as
