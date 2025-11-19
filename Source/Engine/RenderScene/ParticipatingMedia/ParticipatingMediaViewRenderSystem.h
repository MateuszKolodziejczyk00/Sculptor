#pragma once

#include "ViewRenderSystem.h"
#include "SceneRenderer/SceneRenderingTypes.h"
#include "ParticipatingMediaTypes.h"


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

	HeightFogParams GetHeightFogParams() const;

private:

	void RenderParticipatingMedia(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context);

	TextureWithHistory m_directionalLightShadowTerm;

	TextureWithHistory m_localLightsInScattering;

	VolumetricFogParams m_volumetricFogParams;
};

} // spt::rsc
