#pragma once

#include "TextureAssetMacros.h"
#include "Plugins/Plugin.h"
#include "SculptorCoreTypes.h"
#include "StreamableTextureInterface.h"
#include "AssetTypes.h"


namespace spt::rdr
{
class RenderContext;
} // spt::rdr


namespace spt::as
{


struct StreamRequest
{
	AssetHandle                 assetHandle; // reference holder
	StreamableTextureInterface* streamableTexture = nullptr;
};


class TEXTURE_ASSET_API TextureStreamer : public engn::Plugin
{
	SPT_GENERATE_PLUGIN(TextureStreamer);

	using Super = Plugin;

public:

	void RequestTextureUploadToGPU(StreamRequest request);

	void ForceFlushUploads();

protected:

	// Begin Plugin overrides
	virtual void OnPostEngineInit() override;
	virtual void OnBeginFrame(engn::FrameContext& frameContext) override;
	// End Plugin overrides

private:

	void ExecuteUploads();

	void ExecutePreUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext);
	void ExecutePostUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext);


	lib::DynamicArray<StreamRequest> m_requests;
};

} // spt::as
