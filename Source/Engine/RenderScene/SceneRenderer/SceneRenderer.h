#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
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
struct SceneRendererSettings;


class RENDER_SCENE_API SceneRenderer
{
public:

	SceneRenderer();

	rg::RGTextureViewHandle Render(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view, const SceneRendererSettings& settings);

private:

	lib::DynamicArray<ViewRenderingSpec*> CollectRenderViews(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& mainRenderView, SizeType& OUT mainViewIdx) const;
};

} // spt::rsc