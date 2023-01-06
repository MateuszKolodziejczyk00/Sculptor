#include "RenderSystems/StaticMeshRenderSystem.h"
#include "RendererUtils.h"
#include "RenderScene.h"

namespace spt::rsc
{

StaticMeshRenderSystem::StaticMeshRenderSystem()
	: m_basePassInstances(RENDERER_RESOURCE_NAME("BasePassInstancesList"), 1024)
{
	m_supportedStages = ERenderStage::BasePassStage;
}

void StaticMeshRenderSystem::OnInitialize(RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	RenderInstanceData data;
	RenderSceneEntity test = renderScene.CreateEntity(data);

	ecs::Registry& sceneRegistry = renderScene.GetRegistry();
	sceneRegistry.emplace<StaticMeshRenderDataHandle>(test);

	sceneRegistry.on_construct<StaticMeshRenderDataHandle>().connect<&StaticMeshRenderSystem::PostBasePassSMConstructed>(this);
	sceneRegistry.on_destroy<StaticMeshRenderDataHandle>().connect<&StaticMeshRenderSystem::PreBasePassSMDestroyed>(this);
}

void StaticMeshRenderSystem::PostBasePassSMConstructed(ecs::Registry& registry, ecs::Entity entity)
{
	SPT_PROFILER_FUNCTION();

	StaticMeshRenderDataHandle& entityRenderData = registry.get<StaticMeshRenderDataHandle>(entity);
	SPT_CHECK(!entityRenderData.basePassInstanceData.IsValid());

	const StaticMeshRenderData& renderData = registry.get<StaticMeshRenderData>(entity);

	const EntityTransformHandle& transformHandle = registry.get<EntityTransformHandle>(entity);
	const Uint64 transformIdx = transformHandle.transformSuballocation.GetOffset() / sizeof(math::Transform3f);

	StaticMeshGPURenderData gpuRenderData;
	gpuRenderData.firstPrimitiveIdx	= renderData.firstPrimitiveIdx;
	gpuRenderData.primitivesNum		= renderData.primitivesNum;
	gpuRenderData.transformIdx		= static_cast<Uint32>(transformIdx);
	entityRenderData.basePassInstanceData = m_basePassInstances.AddInstance(gpuRenderData);
}

void StaticMeshRenderSystem::PreBasePassSMDestroyed(ecs::Registry& registry, ecs::Entity entity)
{
	SPT_PROFILER_FUNCTION();

	const StaticMeshRenderDataHandle& entityRenderData = registry.get<StaticMeshRenderDataHandle>(entity);
	SPT_CHECK(entityRenderData.basePassInstanceData.IsValid());
	
	m_basePassInstances.RemoveInstance(entityRenderData.basePassInstanceData);
}

} // spt::rsc
