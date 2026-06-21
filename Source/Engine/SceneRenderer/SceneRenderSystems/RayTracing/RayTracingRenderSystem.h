#pragma once

#include "RenderSceneMacros.h"
#include "SceneRenderSystems/SceneRenderSystem.h"
#include "DescriptorSetBindings/AccelerationStructureBinding.h"
#include "RayTracingSceneTypes.h"


namespace spt::rdr
{
class TopLevelAS;
class Buffer;
} // spt::rsc


namespace spt::rsc
{

enum class ETLASGeometryMask : Uint32
{
	None        = 0u,
	Opaque      = BIT(0u),
	Transparent = BIT(1u)
};


class RayTracingRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	static constexpr ESceneRenderSystem systemType = ESceneRenderSystem::RayTracingSystem;

	explicit RayTracingRenderSystem(lib::MemoryArena& arena, RenderScene& owningScene);

	// Begin RenderSceneSubsystem overrides
	void UpdateGPUSceneData(const SceneUpdateContext& context, RenderSceneConstants& sceneData);
	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings);
	// End RenderSceneSubsystem overrides

	const lib::SharedPtr<rdr::TopLevelAS>& GetSceneTLAS() const { return m_tlas; }

private:

	void OnBuildTLAS(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context);

	lib::SharedPtr<rdr::TopLevelAS> m_tlas;

	lib::SharedPtr<rdr::Buffer> m_rtInstancesDataBuffer;
	lib::SharedPtr<rdr::Buffer> m_rtInstancesDataStagingBuffer;

	lib::StaticArray<lib::SharedPtr<rdr::Buffer>, 2u> m_instancesDefsBuffers;
};

} // spt::rsc
