#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "SceneRendererTypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc
{

class RenderScene;
class RenderView;
class ViewRenderingSpec;


DS_BEGIN(SceneRendererDebugDS, rg::RGDescriptorSetState<SceneRendererDebugDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SceneRendererDebugSettings>), u_rendererDebugSettings)
DS_END();


class RENDER_SCENE_API SceneRenderer
{
public:

	SceneRenderer();

	rg::RGTextureViewHandle Render(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view);

	SceneRendererDebugSettings& GetDebugSettings() const;

private:

	lib::DynamicArray<ViewRenderingSpec*> CollectRenderViews(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view, SizeType& OUT mainViewIdx) const;

	lib::SharedRef<SceneRendererDebugDS> CreateDebugDS();

	lib::SharedRef<SceneRendererDebugDS> m_debugDS;
};

} // spt::rsc