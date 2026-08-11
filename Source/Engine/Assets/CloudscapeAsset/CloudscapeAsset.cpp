#include "CloudscapeAsset.h"
#include "AssetsSystem.h"
#include "Engine.h"
#include "CloudscapeCompiler.h"
#include "Transfers/GPUDeferredCommandsQueueTypes.h"
#include "Transfers/GPUDeferredCommandsQueue.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "Utils/TransfersUtils.h"


SPT_DEFINE_LOG_CATEGORY(CloudscapeAsset, true);

namespace spt::as
{

struct CloudscapeDerivedDataHeader
{
	Uint32 version = 0u;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Version", version);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// CloudscapeAssetInitializer ====================================================================

void CloudscapeAssetInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<CloudscapeAssetDefinition>(std::move(m_definition));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CloudscapeTextureUploadRequest ================================================================

class CloudscapeTextureUploadRequest : public gfx::GPUDeferredUploadRequest
{
public:

	CloudscapeTextureUploadRequest() = default;

	// Begin GPUDeferredUploadRequest overrides
	virtual void EnqueueUploads() override;
	// End GPUDeferredUploadRequest overrides

	lib::SharedPtr<rdr::TextureView> dstWeatherMap;
	lib::MTHandle<DDCLoadedBin>      blob;
};


void CloudscapeTextureUploadRequest::EnqueueUploads()
{
	const CompiledCloudscapeHeader& cloudscapeDataHeader = reinterpret_cast<const CompiledCloudscapeHeader&>(*blob->bin.data());

	if (dstWeatherMap && cloudscapeDataHeader.weatherMap.format != rhi::EFragmentFormat::None)
	{
		const rhi::ETextureAspect textureAspect = dstWeatherMap->GetRHI().GetAspect();
		rdr::UploadDataToTexture(blob->bin.data() + cloudscapeDataHeader.weatherMap.dataOffset, cloudscapeDataHeader.weatherMap.dataSize, dstWeatherMap->GetTexture(), textureAspect, dstWeatherMap->GetResolution(), math::Vector3u::Zero(), 0u, 0u);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CloudscapeAsset ===============================================================================

const CloudscapeAssetDefinition& CloudscapeAsset::GetCloudscapeAssetDefinition() const
{
	return GetBlackboard().Get<CloudscapeAssetDefinition>();
}

rsc::CloudscapeDefinition CloudscapeAsset::GetCloudscapeDefinition() const
{
	return rsc::CloudscapeDefinition
	{
		.weatherMap = m_weatherMap
	};
}

Bool CloudscapeAsset::Compile()
{
	SPT_PROFILER_FUNCTION();

	const CloudscapeAssetDefinition& definition = GetBlackboard().Get<CloudscapeAssetDefinition>();

	const std::optional<CloudscapeCompilationResult> compilationResult = cloudscape_compiler::CompileCloudscape(*this, definition);
	if (!compilationResult.has_value())
	{
		SPT_LOG_ERROR(CloudscapeAsset, "Failed to compile CloudscapeAsset '{}'", GetName().ToString());
		return false;
	}

	CloudscapeDerivedDataHeader header;

	CreateDerivedData(*this, header, compilationResult->blob);

	return true;
}

void CloudscapeAsset::OnInitialize()
{
	SPT_PROFILER_FUNCTION();

	const lib::MTHandle<DDCLoadedData<CloudscapeDerivedDataHeader>> compiledData = LoadDerivedData<CloudscapeDerivedDataHeader>(*this);
	SPT_CHECK(compiledData.IsValid());

	const CompiledCloudscapeHeader& cloudscapeDataHeader = reinterpret_cast<const CompiledCloudscapeHeader&>(*compiledData->bin.data());

	if (cloudscapeDataHeader.weatherMap.format != rhi::EFragmentFormat::None)
	{
		rhi::TextureDefinition weatherMapDef;
		weatherMapDef.resolution = cloudscapeDataHeader.weatherMap.resolution;
		weatherMapDef.format     = cloudscapeDataHeader.weatherMap.format;
		weatherMapDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest);
		weatherMapDef.flags      = rhi::ETextureFlags::GloballyReadable;

		m_weatherMap = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("CloudscapeAsset_WeatherMap"), weatherMapDef, rhi::EMemoryUsage::GPUOnly);
	}

	gfx::GPUDeferredCommandsQueue& commandsQueue = engn::GetEngine().GetPluginsManager().GetPluginChecked<gfx::GPUDeferredCommandsQueue>();

	auto uploadRequest = lib::MakeUnique<CloudscapeTextureUploadRequest>();
	uploadRequest->dstWeatherMap = m_weatherMap;
	uploadRequest->blob          = compiledData;

	commandsQueue.RequestUpload(std::move(uploadRequest));
}

} // spt::as
