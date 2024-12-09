#pragma once

#include "SculptorCoreTypes.h"
#include "SculptorDLSSVulkanMacros.h"
#include "RGResources/RGResourceHandles.h"


struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;


namespace spt::rdr
{
class CommandRecorder;
} // spt::rdr


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::dlss
{

struct DLSSParams
{
	math::Vector2u inputResolution  = {};
	math::Vector2u outputResolution = {};
};


struct DLSSRenderingParams
{
	rg::RGTextureViewHandle depth;
	rg::RGTextureViewHandle motion;

	rg::RGTextureViewHandle exposure;

	rg::RGTextureViewHandle inputColor;
	rg::RGTextureViewHandle outputColor;

	math::Vector2f jitter = {};

	Real32 sharpness = 0.0f;

	Bool resetAccumulation = false;
};


class SCULPTORDLSSVULKAN_API SculptorDLSSVulkan
{
public:

	SculptorDLSSVulkan();
	~SculptorDLSSVulkan();

	Bool Initialize();
	void Uninitialize();

	Bool IsInitialized() const;

	Bool IsDirty(const DLSSParams& params) const;

	Bool PrepareForRendering(const DLSSParams& params);
	Bool PrepareForRendering(const rdr::CommandRecorder& cmdRecorder, const DLSSParams& params);

	void Render(rg::RenderGraphBuilder& graphBuilder, const DLSSRenderingParams& renderingParams);

private:

	Bool RequiresFeatureRecreation(const DLSSParams& olddParams, const DLSSParams& newParams) const;

	void ReleaseDLSSFeature();
	void ReleaseNGXResources();

	DLSSParams m_dlssParams;

	NVSDK_NGX_Parameter* m_ngxParams  = nullptr;
	NVSDK_NGX_Handle*    m_dlssHandle = nullptr;

	Bool m_isInitialized = false;
};

} // spt::dlss