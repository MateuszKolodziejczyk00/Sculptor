#pragma once

#include "ViewRenderSystem.h"
#include "RGResources/RGResourceHandles.h"
#include "SceneRenderer/SceneRenderingTypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

struct RenderStageExecutionContext;


struct VolumetricFogParams 
{
	VolumetricFogParams()
		: volumetricFogResolution{}
		, nearPlane(0.f)
		, farPlane(0.f)
	{ }

	rg::RGTextureViewHandle participatingMediaTextureView;
	rg::RGTextureViewHandle indirectInScatteringTextureView;
	rg::RGTextureViewHandle inScatteringTextureView;
	rg::RGTextureViewHandle integratedInScatteringTextureView;

	rg::RGTextureViewHandle directionalLightShadowTerm;
	rg::RGTextureViewHandle historyDirectionalLightShadowTerm;

	math::Vector3u volumetricFogResolution;

	Real32 nearPlane;
	Real32 farPlane;
};


class RENDER_SCENE_API ParticipatingMediaViewRenderSystem : public ViewRenderSystem
{
protected:

	using Super = ViewRenderSystem;

public:

	ParticipatingMediaViewRenderSystem();

	// Begin ViewRenderSystem interface
	virtual void BeginFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) override;
	// End ViewRenderSystem interface

	const VolumetricFogParams& GetVolumetricFogParams() const;

private:

	void RenderParticipatingMedia(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

	TextureWithHistory m_directionalLightShadowTerm;

	VolumetricFogParams m_volumetricFogParams;
};

} // spt::rsc
