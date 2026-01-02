#include "SculptorDLSSVulkan.h"
#include "RHIBridge/RHIImpl.h"
#include "RHICore/RHIPlugin.h"
#include "Paths.h"
#include "Utility/String/StringUtils.h"

#include "RHI/Vulkan/Device/LogicalDevice.h"
#include "RHI/Vulkan/VulkanRHIUtils.h"

#include "CommandsRecorder/CommandRecorder.h"
#include "Types/CommandBuffer.h"

#include "ResourcesManager.h"
#include "Renderer.h"

#include "RenderGraphBuilder.h"
#include "RGNodeParametersStruct.h"

#include "nvsdk_ngx_vk.h"
#include "nvsdk_ngx_helpers_vk.h"
#include "nvsdk_ngx_helpers_dlssd_vk.h"


SPT_DEFINE_LOG_CATEGORY(DLSS, true);


namespace spt::dlss
{

namespace constants
{
// ID from example app
constexpr Uint64 dlssApplicationID = 231313132u;
} // constants

namespace priv
{


NVSDK_NGX_PerfQuality_Value ToNGXQuality(EDLSSQuality quality)
{
	switch (quality)
	{
	case EDLSSQuality::Low:    return NVSDK_NGX_PerfQuality_Value_MaxPerf;
	case EDLSSQuality::Medium: return NVSDK_NGX_PerfQuality_Value_Balanced;
	case EDLSSQuality::Ultra:  return NVSDK_NGX_PerfQuality_Value_MaxQuality;
	case EDLSSQuality::DLAA:   return NVSDK_NGX_PerfQuality_Value_DLAA;
	default:
		SPT_CHECK_NO_ENTRY();
	}

	return NVSDK_NGX_PerfQuality_Value_MaxQuality;
}


NVSDK_NGX_DLSS_Feature_Flags ToNGXFlags(EDLSSFlags flags)
{
	NVSDK_NGX_DLSS_Feature_Flags ngxFlags = NVSDK_NGX_DLSS_Feature_Flags_None;

	if (lib::HasAnyFlag(flags, EDLSSFlags::HDR))
	{
		lib::AddFlags(ngxFlags, NVSDK_NGX_DLSS_Feature_Flags_IsHDR);
	}

	if (lib::HasAnyFlag(flags, EDLSSFlags::ReversedDepth))
	{
		lib::AddFlags(ngxFlags, NVSDK_NGX_DLSS_Feature_Flags_DepthInverted);
	}

	if (lib::HasAnyFlag(flags, EDLSSFlags::MotionLowRes))
	{
		lib::AddFlags(ngxFlags, NVSDK_NGX_DLSS_Feature_Flags_MVLowRes);
	}

	return ngxFlags;
}


class DLSSRHIPlugin : public rhi::RHIPlugin
{
protected:

	using Super = rhi::RHIPlugin;

public:

	// Begin rhi::RHIExtensionsProvider overrides
	virtual void CollectExtensions(rhi::ExtensionsCollector& collector) const override;
	// End rhi::RHIExtensionsProvider overrides
};


void DLSSRHIPlugin::CollectExtensions(rhi::ExtensionsCollector& collector) const
{
	Super::CollectExtensions(collector);

	const char** deviceExtensions = nullptr;
	unsigned int deviceExtensionsCount = 0;

	const char** instanceExtensions = nullptr;
	unsigned int instanceExtensionsCount = 0;

	NVSDK_NGX_VULKAN_RequiredExtensions(OUT &instanceExtensionsCount, OUT &instanceExtensions, OUT &deviceExtensionsCount, OUT &deviceExtensions);

	for (unsigned int i = 0; i < instanceExtensionsCount; ++i)
	{
		collector.AddRHIExtension(lib::StringView(instanceExtensions[i]));
	}

	for (unsigned int i = 0; i < deviceExtensionsCount; ++i)
	{
		collector.AddDeviceExtension(lib::StringView(deviceExtensions[i]));
	}
}

DLSSRHIPlugin g_dlssRHIPlugin;


BEGIN_RG_NODE_PARAMETERS_STRUCT(DLSSRenderNodeParams)
	RG_TEXTURE_VIEW(input,    rg::ERGTextureAccess::ShaderRead,  rhi::EPipelineStage::ComputeShader)
	RG_TEXTURE_VIEW(output,   rg::ERGTextureAccess::ShaderWrite, rhi::EPipelineStage::ComputeShader)
	RG_TEXTURE_VIEW(depth,    rg::ERGTextureAccess::ShaderRead,  rhi::EPipelineStage::ComputeShader)
	RG_TEXTURE_VIEW(motion,   rg::ERGTextureAccess::ShaderRead,  rhi::EPipelineStage::ComputeShader)
	RG_TEXTURE_VIEW(exposure, rg::ERGTextureAccess::ShaderRead,  rhi::EPipelineStage::ComputeShader)
END_RG_NODE_PARAMETERS_STRUCT();


BEGIN_RG_NODE_PARAMETERS_STRUCT(DLSSRayReconstructionParams)
	RG_TEXTURE_VIEW(diffuseAlbedo,       rg::ERGTextureAccess::ShaderRead,  rhi::EPipelineStage::ComputeShader)
	RG_TEXTURE_VIEW(specularAlbedo,      rg::ERGTextureAccess::ShaderWrite, rhi::EPipelineStage::ComputeShader)
	RG_TEXTURE_VIEW(normals,             rg::ERGTextureAccess::ShaderRead,  rhi::EPipelineStage::ComputeShader)
	RG_TEXTURE_VIEW(roughness,           rg::ERGTextureAccess::ShaderRead,  rhi::EPipelineStage::ComputeShader)
	RG_TEXTURE_VIEW(specularHitDistance, rg::ERGTextureAccess::ShaderRead,  rhi::EPipelineStage::ComputeShader)
END_RG_NODE_PARAMETERS_STRUCT()


NVSDK_NGX_Resource_VK CreateNGXTexture(const rhi::RHITextureView& rhiView)
{
	SPT_CHECK(rhiView.IsValid());

	const rhi::RHITexture* rhiTexture = rhiView.GetTexture();
	SPT_CHECK(!!rhiTexture);
	SPT_CHECK(rhiTexture->IsValid());

	const rhi::TextureSubresourceRange& subresourceRange = rhiView.GetSubresourceRange();

	NVSDK_NGX_Resource_VK resource;
	resource.Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;
	resource.Resource.ImageViewInfo.ImageView                       = rhiView.GetHandle();
	resource.Resource.ImageViewInfo.Image                           = rhiTexture->GetHandle();
	resource.Resource.ImageViewInfo.SubresourceRange.aspectMask     = rhi::RHIToVulkan::GetAspectFlags(subresourceRange.aspect);
	resource.Resource.ImageViewInfo.SubresourceRange.baseMipLevel   = subresourceRange.baseMipLevel;
	resource.Resource.ImageViewInfo.SubresourceRange.levelCount     = subresourceRange.mipLevelsNum;
	resource.Resource.ImageViewInfo.SubresourceRange.baseArrayLayer = subresourceRange.baseArrayLayer;
	resource.Resource.ImageViewInfo.SubresourceRange.layerCount     = subresourceRange.arrayLayersNum;
	resource.Resource.ImageViewInfo.Format                          = rhi::RHITexture::GetVulkanFormat(rhiTexture->GetFormat());
	resource.Resource.ImageViewInfo.Width                           = rhiTexture->GetResolution().x();
	resource.Resource.ImageViewInfo.Height                          = rhiTexture->GetResolution().y();
	resource.ReadWrite                                              = lib::HasAnyFlag(rhiTexture->GetDefinition().usage, rhi::ETextureUsage::StorageTexture);

	return resource;
}


void EvaluateDLSS(rdr::CommandRecorder& commandRecorder, const DLSSRenderingParams& renderingParams, const DLSSParams& dlssParams, NVSDK_NGX_Handle& dlssHandle, NVSDK_NGX_Parameter& ngxParams)
{
	NVSDK_NGX_Resource_VK depthResource    = CreateNGXTexture(renderingParams.depth->GetRHI());
	NVSDK_NGX_Resource_VK motionResource   = CreateNGXTexture(renderingParams.motion->GetRHI());
	NVSDK_NGX_Resource_VK exposureResource = CreateNGXTexture(renderingParams.exposure->GetRHI());
	NVSDK_NGX_Resource_VK inputResouce     = CreateNGXTexture(renderingParams.inputColor->GetRHI());
	NVSDK_NGX_Resource_VK outputResouce    = CreateNGXTexture(renderingParams.outputColor->GetRHI());

	const Real32 resolutionX = static_cast<Real32>(dlssParams.inputResolution.x());
	const Real32 resolutionY = static_cast<Real32>(dlssParams.inputResolution.y());

	NVSDK_NGX_VK_DLSS_Eval_Params dlssEvalParams{};
	dlssEvalParams.pInDepth                         = &depthResource;
	dlssEvalParams.pInMotionVectors                 = &motionResource;
	dlssEvalParams.InJitterOffsetX                  = renderingParams.jitter.x() * resolutionX;
	dlssEvalParams.InJitterOffsetY                  = renderingParams.jitter.y() * resolutionY;
	dlssEvalParams.InReset                          = renderingParams.resetAccumulation ? 1 : 0;
	dlssEvalParams.InMVScaleX                       = -resolutionX;
	dlssEvalParams.InMVScaleY                       = -resolutionY;
	dlssEvalParams.pInExposureTexture               = &exposureResource;
	dlssEvalParams.InRenderSubrectDimensions.Width  = dlssParams.inputResolution.x();
	dlssEvalParams.InRenderSubrectDimensions.Height = dlssParams.inputResolution.y();

	dlssEvalParams.Feature.InSharpness = renderingParams.sharpness;
	dlssEvalParams.Feature.pInColor    = &inputResouce;
	dlssEvalParams.Feature.pInOutput   = &outputResouce;

	const rhi::RHICommandBuffer& rhiCmdBuffer = commandRecorder.GetCommandBuffer()->GetRHI();

	const NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSS_EXT(rhiCmdBuffer.GetHandle(), &dlssHandle, &ngxParams, &dlssEvalParams);

	if (result != NVSDK_NGX_Result_Success)
	{
		SPT_LOG_ERROR(DLSS, "Failed to evaluate DLSS. Reason: {}", lib::StringUtils::ToMultibyteString(GetNGXResultAsString(result)));
	}
}


void EvaluateDLSSRR(rdr::CommandRecorder& commandRecorder, const DLSSRenderingParams& renderingParams, const DLSSRayReconstructionParams& rayReconstructionParams, const DLSSParams& dlssParams, NVSDK_NGX_Handle& dlssHandle, NVSDK_NGX_Parameter& ngxParams)
{
	NVSDK_NGX_Resource_VK depthResource    = CreateNGXTexture(renderingParams.depth->GetRHI());
	NVSDK_NGX_Resource_VK motionResource   = CreateNGXTexture(renderingParams.motion->GetRHI());
	NVSDK_NGX_Resource_VK exposureResource = CreateNGXTexture(renderingParams.exposure->GetRHI());
	NVSDK_NGX_Resource_VK inputResouce     = CreateNGXTexture(renderingParams.inputColor->GetRHI());
	NVSDK_NGX_Resource_VK outputResouce    = CreateNGXTexture(renderingParams.outputColor->GetRHI());

	NVSDK_NGX_Resource_VK diffuseAlbedoResource       = CreateNGXTexture(rayReconstructionParams.diffuseAlbedo->GetRHI());
	NVSDK_NGX_Resource_VK specularAlbedoResource      = CreateNGXTexture(rayReconstructionParams.specularAlbedo->GetRHI());
	NVSDK_NGX_Resource_VK normalsResource             = CreateNGXTexture(rayReconstructionParams.normals->GetRHI());
	NVSDK_NGX_Resource_VK roughnessResource           = CreateNGXTexture(rayReconstructionParams.roughness->GetRHI());
	NVSDK_NGX_Resource_VK specularHitDistanceResource = CreateNGXTexture(rayReconstructionParams.specularHitDistance->GetRHI());

	const Real32 resolutionX = static_cast<Real32>(dlssParams.inputResolution.x());
	const Real32 resolutionY = static_cast<Real32>(dlssParams.inputResolution.y());

	math::Matrix4f worldToView = renderingParams.rayReconstructionParams->worldToView;
	math::Matrix4f viewToClip = renderingParams.rayReconstructionParams->viewToClip;

	NVSDK_NGX_VK_DLSSD_Eval_Params dlssEvalParams{};
	dlssEvalParams.pInDepth                         = &depthResource;
	dlssEvalParams.pInMotionVectors                 = &motionResource;
	dlssEvalParams.InJitterOffsetX                  = renderingParams.jitter.x() * resolutionX;
	dlssEvalParams.InJitterOffsetY                  = renderingParams.jitter.y() * resolutionY;
	dlssEvalParams.InReset                          = renderingParams.resetAccumulation ? 1 : 0;
	dlssEvalParams.InMVScaleX                       = -resolutionX;
	dlssEvalParams.InMVScaleY                       = -resolutionY;
	dlssEvalParams.pInExposureTexture               = &exposureResource;
	dlssEvalParams.InRenderSubrectDimensions.Width  = dlssParams.inputResolution.x();
	dlssEvalParams.InRenderSubrectDimensions.Height = dlssParams.inputResolution.y();

	dlssEvalParams.pInWorldToViewMatrix = worldToView.data();
	dlssEvalParams.pInViewToClipMatrix  = viewToClip.data();

    dlssEvalParams.pInDiffuseAlbedo       = &diffuseAlbedoResource;
    dlssEvalParams.pInSpecularAlbedo      = &specularAlbedoResource;
    dlssEvalParams.pInNormals             = &normalsResource;
    dlssEvalParams.pInRoughness           = &roughnessResource;
	dlssEvalParams.pInSpecularHitDistance = &specularHitDistanceResource;

	dlssEvalParams.pInColor    = &inputResouce;
	dlssEvalParams.pInOutput   = &outputResouce;

	const rhi::RHICommandBuffer& rhiCmdBuffer = commandRecorder.GetCommandBuffer()->GetRHI();

	const NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSSD_EXT(rhiCmdBuffer.GetHandle(), &dlssHandle, &ngxParams, &dlssEvalParams);

	if (result != NVSDK_NGX_Result_Success)
	{
		SPT_LOG_ERROR(DLSS, "Failed to evaluate DLSS. Reason: {}", lib::StringUtils::ToMultibyteString(GetNGXResultAsString(result)));
	}
}

} // priv


namespace ngx
{

class NGXInstance
{
public:

	static NGXInstance& GetInstance()
	{
		static NGXInstance instance;
		return instance;
	}

	Bool TryInitialize()
	{
		if (!IsInitialized())
		{
			return Initialize();
		}

		return true;
	}

	Bool IsInitialized() const
	{
		return m_isInitialized;
	}

private:

	NGXInstance() = default;

	Bool Initialize();
	void Uninitialize();

	Bool m_isInitialized = false;
};

Bool NGXInstance::Initialize()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!m_isInitialized);

	lib::String dlssDataPath = engn::Paths::GetExecutablePath();

	const SizeType lastSlashIndex = dlssDataPath.find_last_of('\\');
	if (lastSlashIndex != lib::String::npos)
	{
		dlssDataPath.erase(dlssDataPath.begin() + lastSlashIndex, dlssDataPath.end());
	}

	if (dlssDataPath.empty())
	{
		return false;
	}

	const lib::WString dlssDataPathW = lib::StringUtils::ToWideString(dlssDataPath);

	const NVSDK_NGX_Result initResult = NVSDK_NGX_VULKAN_Init(constants::dlssApplicationID, dlssDataPathW.c_str(), rhi::RHI::GetInstanceHandle(), rhi::RHI::GetPhysicalDeviceHandle(), rhi::RHI::GetDeviceHandle());

	if (initResult != NVSDK_NGX_Result_Success)
	{
		SPT_LOG_ERROR(DLSS, "Failed to initialize NGX. Reason: {}", lib::StringUtils::ToMultibyteString(GetNGXResultAsString(initResult)));
		return false;
	}

	m_isInitialized = true;

	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda(
		[this]()
		{
			Uninitialize();
		});

	SPT_CHECK(IsInitialized());
	return true;
}

void NGXInstance::Uninitialize()
{
	SPT_PROFILER_FUNCTION();

	NVSDK_NGX_VULKAN_Shutdown();
}

} // ngx


Bool SculptorDLSSVulkan::InitializeDLSS()
{
	return ngx::NGXInstance::GetInstance().TryInitialize();
}

SculptorDLSSVulkan::SculptorDLSSVulkan()
	: m_isInitialized(false)
{
}

SculptorDLSSVulkan::~SculptorDLSSVulkan()
{
	if (IsInitialized())
	{
		Uninitialize();
	}

	SPT_CHECK(!IsInitialized());
}

Bool SculptorDLSSVulkan::Initialize()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsInitialized());

	const Bool ngxInitialized = ngx::NGXInstance::GetInstance().TryInitialize();
	if (!ngxInitialized)
	{
		return false;
	}

	const NVSDK_NGX_Result capabilityParamsResult = NVSDK_NGX_VULKAN_GetCapabilityParameters(&m_ngxParams);

	if (capabilityParamsResult != NVSDK_NGX_Result_Success)
	{
		SPT_LOG_ERROR(DLSS, "Failed to fetch NGX parameters. Reason: {}", lib::StringUtils::ToMultibyteString(GetNGXResultAsString(capabilityParamsResult)));
		return false;
	}

	int isSuperSamplingAvailable = 0;
	const NVSDK_NGX_Result superResolutionAvailableResult = m_ngxParams->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &isSuperSamplingAvailable);
	if (superResolutionAvailableResult != NVSDK_NGX_Result_Success)
	{
		SPT_LOG_ERROR(DLSS, "Failed to check if Super Resolution is available. Reason: {}", lib::StringUtils::ToMultibyteString(GetNGXResultAsString(superResolutionAvailableResult)));
		return false;
	}

	if (isSuperSamplingAvailable == 0)
	{
		SPT_LOG_ERROR(DLSS, "Super Resolution is not available.");
		return false;
	}

	int isSuperSamplingDenoisingAvailable = 0;
	const NVSDK_NGX_Result superResolutionDenoisingAvailableResult = m_ngxParams->Get(NVSDK_NGX_Parameter_SuperSamplingDenoising_Available, &isSuperSamplingDenoisingAvailable);
	if (superResolutionDenoisingAvailableResult != NVSDK_NGX_Result_Success)
	{
		SPT_LOG_ERROR(DLSS, "Failed to check if Super Resolution Denoising is available. Reason: {}", lib::StringUtils::ToMultibyteString(GetNGXResultAsString(superResolutionDenoisingAvailableResult)));
		return false;
	}

	if (isSuperSamplingDenoisingAvailable == 0)
	{
		SPT_LOG_ERROR(DLSS, "Super Resolution Denoising is not available.");
		return false;
	}

	m_isInitialized = true;

	SPT_CHECK(IsInitialized());

	return true;
}

void SculptorDLSSVulkan::Uninitialize()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsInitialized());

	if (m_dlssHandle)
	{
		ReleaseDLSSFeature();
	}

	ReleaseNGXResources();

	m_isInitialized = false;

	SPT_CHECK(!IsInitialized());
}

Bool SculptorDLSSVulkan::IsInitialized() const
{
	return m_isInitialized;
}

Bool SculptorDLSSVulkan::IsDirty(const DLSSParams& params) const
{
	SPT_CHECK(IsInitialized());

	return RequiresFeatureRecreation(params, m_dlssParams);
}

Bool SculptorDLSSVulkan::PrepareForRendering(const DLSSParams& params)
{
	Bool result = true;

	if (IsDirty(params))
	{
		lib::SharedRef<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("DLSS Preparation Render Context"));

		rhi::CommandBufferDefinition cmdBufferDef;
		cmdBufferDef.complexityClass = rhi::ECommandBufferComplexityClass::Low;

		const lib::UniquePtr<rdr::CommandRecorder> cmdRecorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("DLSS Preparation Cmd Recorder"),
																											  renderContext,
																											  cmdBufferDef);

		result = PrepareForRendering(*cmdRecorder, params);

		lib::SharedRef<rdr::GPUWorkload> workload = cmdRecorder->FinishRecording();

		rdr::Renderer::GetDeviceQueuesManager().Submit(workload, rdr::EGPUWorkloadSubmitFlags::CorePipe);
	}

	return result;
}

Bool SculptorDLSSVulkan::PrepareForRendering(const rdr::CommandRecorder& cmdRecorder, const DLSSParams& params)
{
	SPT_PROFILER_FUNCTION();

	if (!IsInitialized())
	{
		return false;
	}

	const Bool requiresRecreation = RequiresFeatureRecreation(m_dlssParams, params);

	Bool featureCreated = !requiresRecreation;

	if (requiresRecreation)
	{
		if (m_dlssHandle)
		{
			ReleaseDLSSFeature();
		}

		if (params.enableRayReconstruction)
		{
			const NVSDK_NGX_RayReconstruction_Hint_Render_Preset newPreset = NVSDK_NGX_RayReconstruction_Hint_Render_Preset::NVSDK_NGX_RayReconstruction_Hint_Render_Preset_Default;

			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Balanced, newPreset);
			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Quality, newPreset);
			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Balanced, newPreset);
			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Performance, newPreset);
			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_UltraPerformance, newPreset);
			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_UltraQuality, newPreset);
		}
		else
		{
			const NVSDK_NGX_DLSS_Hint_Render_Preset newPreset = params.enableTransformerModel 
				                                              ? NVSDK_NGX_DLSS_Hint_Render_Preset::NVSDK_NGX_DLSS_Hint_Render_Preset_J
				                                              : NVSDK_NGX_DLSS_Hint_Render_Preset::NVSDK_NGX_DLSS_Hint_Render_Preset_Default;

			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, newPreset);
			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, newPreset);
			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, newPreset);
			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, newPreset);
			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, newPreset);
			NVSDK_NGX_Parameter_SetUI(m_ngxParams, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraQuality, newPreset);
		}

		if (params.enableRayReconstruction)
		{
			featureCreated = CreateDLLSRRFeature(cmdRecorder, params);
		}
		else
		{
			featureCreated = CreateDLLSFeature(cmdRecorder, params);
		}
	}

	if (featureCreated)
	{
		m_dlssParams = params;
	}

	return m_dlssHandle != nullptr;
}

void SculptorDLSSVulkan::Render(rg::RenderGraphBuilder& graphBuilder, const DLSSRenderingParams& renderingParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsInitialized());

	SPT_CHECK(!!m_dlssHandle);
	SPT_CHECK(!!m_ngxParams);

	SPT_CHECK(!m_dlssParams.enableRayReconstruction || !!renderingParams.rayReconstructionParams)

	priv::DLSSRenderNodeParams dlssNodeParams;
	dlssNodeParams.input    = renderingParams.inputColor;
	dlssNodeParams.output   = renderingParams.outputColor;
	dlssNodeParams.depth    = renderingParams.depth;
	dlssNodeParams.motion   = renderingParams.motion;
	dlssNodeParams.exposure = renderingParams.exposure;

	priv::DLSSRayReconstructionParams rayReconstructionParams;
	if (m_dlssParams.enableRayReconstruction)
	{
		const DLSSRayReconstructionRenderingParams& rayReconstructionParamsData = *renderingParams.rayReconstructionParams;

		rayReconstructionParams.diffuseAlbedo       = rayReconstructionParamsData.diffuseAlbedo;
		rayReconstructionParams.specularAlbedo      = rayReconstructionParamsData.specularAlbedo;
		rayReconstructionParams.normals             = rayReconstructionParamsData.normals;
		rayReconstructionParams.roughness           = rayReconstructionParamsData.roughness;
		rayReconstructionParams.specularHitDistance = rayReconstructionParamsData.specularHitDistance;
	}

	renderingParams.outputColor->GetTexture()->TryAppendUsage(rhi::ETextureUsage::TransferDest);

	if (m_dlssParams.enableRayReconstruction)
	{
		graphBuilder.AddLambdaPass(RG_DEBUG_NAME("DLSS RR: Evaluate"),
								   std::tie(dlssNodeParams, rayReconstructionParams),
								   [renderingParams, rayReconstructionParams, dlssParams = m_dlssParams, dlssHandle = m_dlssHandle, ngxParams = m_ngxParams]
								   (const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								   {
									   priv::EvaluateDLSSRR(recorder, renderingParams, rayReconstructionParams, dlssParams, *dlssHandle, *ngxParams);
								   });
	}
	else
	{
		graphBuilder.AddLambdaPass(RG_DEBUG_NAME("DLSS: Evaluate"),
								   std::tie(dlssNodeParams),
								   [renderingParams, dlssParams = m_dlssParams, dlssHandle = m_dlssHandle, ngxParams = m_ngxParams]
								   (const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
								   {
									   priv::EvaluateDLSS(recorder, renderingParams, dlssParams, *dlssHandle, *ngxParams);
								   });
	}
}

Bool SculptorDLSSVulkan::RequiresFeatureRecreation(const DLSSParams& oldParams, const DLSSParams& newParams) const
{
	SPT_CHECK(IsInitialized());

	return oldParams != newParams;
}

Bool SculptorDLSSVulkan::CreateDLLSFeature(const rdr::CommandRecorder& cmdRecorder, const DLSSParams& params)
{
	SPT_CHECK(!m_dlssHandle);

	NVSDK_NGX_DLSS_Create_Params dlssParams;
	dlssParams.Feature.InWidth            = params.inputResolution.x();
	dlssParams.Feature.InHeight           = params.inputResolution.y();
	dlssParams.Feature.InTargetWidth      = params.outputResolution.x();
	dlssParams.Feature.InTargetHeight     = params.outputResolution.y();
	dlssParams.Feature.InPerfQualityValue = priv::ToNGXQuality(params.quality);
	dlssParams.InFeatureCreateFlags       = priv::ToNGXFlags(params.flags);
	dlssParams.InEnableOutputSubrects     = false;

	const lib::SharedPtr<rdr::CommandBuffer>& cmdBuffer = cmdRecorder.GetCommandBuffer();
	const rhi::RHICommandBuffer& rhiCmdBuffer = cmdBuffer->GetRHI();

	// Only for multi-GPU configurations
	const Uint32 creationNodeMask   = 1u;
	const Uint32 visibilityNodeMask = 1u;

	const NVSDK_NGX_Result createDLSSResult = NGX_VULKAN_CREATE_DLSS_EXT(rhiCmdBuffer.GetHandle(), creationNodeMask, visibilityNodeMask, OUT &m_dlssHandle, m_ngxParams, &dlssParams);

	if (createDLSSResult != NVSDK_NGX_Result_Success)
	{
		SPT_LOG_ERROR(DLSS, "Failed to create DLSS feature. Reason: {}", lib::StringUtils::ToMultibyteString(GetNGXResultAsString((createDLSSResult))));
	}
	else
	{
		SPT_CHECK(m_dlssHandle != nullptr);
	}

	return m_dlssHandle != nullptr;
}

Bool SculptorDLSSVulkan::CreateDLLSRRFeature(const rdr::CommandRecorder& cmdRecorder, const DLSSParams& params)
{
	SPT_CHECK(!m_dlssHandle);

	NVSDK_NGX_DLSSD_Create_Params dlssParams{};
    dlssParams.InDenoiseMode        = NVSDK_NGX_DLSS_Denoise_Mode_DLUnified;
    dlssParams.InRoughnessMode      = NVSDK_NGX_DLSS_Roughness_Mode_Unpacked;
    dlssParams.InUseHWDepth         = NVSDK_NGX_DLSS_Depth_Type_HW;
    dlssParams.InWidth              = params.inputResolution.x();
    dlssParams.InHeight             = params.inputResolution.y();
    dlssParams.InTargetWidth        = params.outputResolution.x();
    dlssParams.InTargetHeight       = params.outputResolution.y();
	dlssParams.InPerfQualityValue   = priv::ToNGXQuality(params.quality);
	dlssParams.InFeatureCreateFlags = priv::ToNGXFlags(params.flags);

	const lib::SharedPtr<rdr::CommandBuffer>& cmdBuffer = cmdRecorder.GetCommandBuffer();
	const rhi::RHICommandBuffer& rhiCmdBuffer = cmdBuffer->GetRHI();

	// Only for multi-GPU configurations
	const uint32_t creationNodeMask   = 1u;
	const uint32_t visibilityNodeMask = 1u;

	const NVSDK_NGX_Result createDLSSResult = NGX_VULKAN_CREATE_DLSSD_EXT1(VK_NULL_HANDLE, rhiCmdBuffer.GetHandle(), creationNodeMask, visibilityNodeMask, OUT &m_dlssHandle, m_ngxParams, &dlssParams);

	if (createDLSSResult != NVSDK_NGX_Result_Success)
	{
		SPT_LOG_ERROR(DLSS, "Failed to create DLSS RR feature. Reason: {}", lib::StringUtils::ToMultibyteString(GetNGXResultAsString((createDLSSResult))));
	}
	else
	{
		SPT_CHECK(m_dlssHandle != nullptr);
	}

	return m_dlssHandle != nullptr;
}

void SculptorDLSSVulkan::ReleaseDLSSFeature()
{
	SPT_CHECK(!!m_dlssHandle);

	rdr::Renderer::WaitIdle(false);
	const NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_ReleaseFeature(m_dlssHandle);

	if (result == NVSDK_NGX_Result_Success)
	{
		m_dlssHandle = nullptr;
	}
	else
	{
		SPT_CHECK_MSG(false, "Failed to release DLSS feature. Reason: {}", lib::StringUtils::ToMultibyteString(GetNGXResultAsString(result)));
	}
}

void SculptorDLSSVulkan::ReleaseNGXResources()
{
	SPT_CHECK(!!m_ngxParams);

	const NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_DestroyParameters(m_ngxParams);

	if (result == NVSDK_NGX_Result_Success)
	{
		m_ngxParams = nullptr;
	}
	else
	{
		SPT_CHECK_MSG(false, "Failed to destroy NGX parameters. Reason: {}", lib::StringUtils::ToMultibyteString(GetNGXResultAsString(result)));
	}
}

} // spt::dlss
