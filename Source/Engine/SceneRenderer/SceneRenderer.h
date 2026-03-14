#pragma once

#include "SceneRendererMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "SceneRendererTypes.h"
#include "Utility/ModuleDescriptor.h"
#include "Utility/NamedType.h"
#include "Utility/Templates/Callable.h"


namespace spt::engn
{
struct EngineGlobals;
}; // spt::engn


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


using SceneRendererHandle = lib::NamedType<Uint64, struct SceneRendererHandleTag>;

RenderView* CreateRenderView(const RenderViewDefinition& definition);
void        DestroyRenderView(RenderView*& view);

SceneRendererHandle CreateSceneRenderer(const SceneRendererDefinition& definition);
void                DestroySceneRenderer(SceneRendererHandle& handle);

void                             SetShadingViewSettings(RenderView& view, const ShadingRenderViewSettings& settings);
const ShadingRenderViewSettings& GetShadingViewSettings(RenderView& view);

rg::RGTextureViewHandle ExecuteSceneRendering(SceneRendererHandle renderer, rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view, const SceneRendererSettings& settings);

void TempDrawParamtersUI(void* ctx);

} // spt::rsc


extern "C"
{
SCENE_RENDERER_API void InitializeModule(const spt::engn::EngineGlobals& engineGlobals, spt::lib::ModuleDescriptor& outModuleDescriptor);
}

struct SceneRendererDLLModuleAPI
{
	spt::lib::RawCallable<spt::rsc::RenderView*(const spt::rsc::RenderViewDefinition& /* definition */)> CreateRenderView;
	spt::lib::RawCallable<void(spt::rsc::RenderView*& /* view */)> DestroyRenderView;
	spt::lib::RawCallable<void(spt::rsc::RenderView&, const spt::rsc::ShadingRenderViewSettings&)> SetShadingViewSettings;
	spt::lib::RawCallable<const spt::rsc::ShadingRenderViewSettings&(spt::rsc::RenderView&)> GetShadingViewSettings;
	spt::lib::RawCallable<spt::rsc::SceneRendererHandle(const spt::rsc::SceneRendererDefinition& /* definition */)> CreateSceneRenderer;
	spt::lib::RawCallable<void(spt::rsc::SceneRendererHandle& /* handle */)> DestroySceneRenderer;
	spt::lib::RawCallable<spt::rg::RGTextureViewHandle(spt::rsc::SceneRendererHandle /* renderer */, spt::rg::RenderGraphBuilder& /* graphBuilder */, spt::rsc::RenderScene& /* scene */, spt::rsc::RenderView& /* view */, const spt::rsc::SceneRendererSettings& /* settings */)> ExecuteSceneRendering;
	spt::lib::RawCallable<void(void* /* ctx */)> DrawParametersUI;
	spt::lib::RawCallable<void(void* /* ctx */)> DrawRendererStatsUI;
};
