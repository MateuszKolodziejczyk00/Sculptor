#pragma once

#include "ViewRenderSystems/ViewRenderSystem.h"
#include "ParticipatingMediaTypes.h"
#include "Utils/SceneRenderingTypes.h"


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

class RenderViewEntryContext;


class RENDER_SCENE_API ParticipatingMediaViewRenderSystem : public ViewRenderSystem
{
protected:

	using Super = ViewRenderSystem;

public:

	static constexpr EViewRenderSystem type  = EViewRenderSystem::ParticipatingMediaSystem;

	explicit ParticipatingMediaViewRenderSystem(RenderView& inRenderView);

	// Begin ViewRenderSystem interface
	void BeginFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
	// End ViewRenderSystem interface

	const VolumetricFogParams& GetVolumetricFogParams() const;

	HeightFogParams GetHeightFogParams() const;

private:

	void RenderParticipatingMedia(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context);

	TextureWithHistory m_directionalLightShadowTerm;

	TextureWithHistory m_localLightsInScattering;

	VolumetricFogParams m_volumetricFogParams;
};

} // spt::rsc
