#include "StaticMeshPrimitivesSystem.h"
#include "RendererUtils.h"
#include "RenderScene.h"

namespace spt::rsc
{

StaticMeshPrimitivesSystem::StaticMeshPrimitivesSystem(RenderScene& owningScene)
	: Super(owningScene)
	, m_staticMeshInstances(RENDERER_RESOURCE_NAME("StaticMeshInstancesList"), 1024)
{
	ecs::Registry& sceneRegistry = owningScene.GetRegistry();
	sceneRegistry.on_construct<StaticMeshInstanceRenderData>().connect<&StaticMeshPrimitivesSystem::OnStaticMeshUpdated>(this);
	sceneRegistry.on_update<StaticMeshInstanceRenderData>().connect<&StaticMeshPrimitivesSystem::OnStaticMeshUpdated>(this);
	sceneRegistry.on_destroy<StaticMeshInstanceRenderData>().connect<&StaticMeshPrimitivesSystem::OnStaticMeshDestryed>(this);
}

StaticMeshPrimitivesSystem::~StaticMeshPrimitivesSystem()
{
	RenderScene& owningScene = GetOwningScene();
	ecs::Registry& sceneRegistry = owningScene.GetRegistry();
	sceneRegistry.on_construct<StaticMeshInstanceRenderData>().disconnect<&StaticMeshPrimitivesSystem::OnStaticMeshUpdated>(this);
	sceneRegistry.on_update<StaticMeshInstanceRenderData>().disconnect<&StaticMeshPrimitivesSystem::OnStaticMeshUpdated>(this);
	sceneRegistry.on_destroy<StaticMeshInstanceRenderData>().disconnect<&StaticMeshPrimitivesSystem::OnStaticMeshDestryed>(this);
}

void StaticMeshPrimitivesSystem::Update()
{
	SPT_PROFILER_FUNCTION();

	Super::Update();

	m_staticMeshInstances.FlushRemovedInstances();
}

const StaticMeshInstancesList& StaticMeshPrimitivesSystem::GetStaticMeshInstances() const
{
	return m_staticMeshInstances;
}

void StaticMeshPrimitivesSystem::OnStaticMeshUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	SPT_PROFILER_FUNCTION();

	const StaticMeshInstanceRenderData& instanceRenderData = registry.get<StaticMeshInstanceRenderData>(entity);
	
	StaticMeshRenderDataHandle& entityGPURenderDataHandle = registry.get_or_emplace<StaticMeshRenderDataHandle>(entity);

	if (entityGPURenderDataHandle.staticMeshGPUInstanceData.IsValid())
	{
		// Destroy old data
		m_staticMeshInstances.RemoveInstance(entityGPURenderDataHandle.staticMeshGPUInstanceData);
	}

	const EntityTransformHandle& transformHandle = registry.get<EntityTransformHandle>(entity);
	const Uint64 transformIdx = transformHandle.transformSuballocation.GetOffset() / sizeof(math::Affine3f);

	StaticMeshGPUInstanceRenderData gpuRenderData;
	gpuRenderData.transformIdx	= static_cast<Uint32>(transformIdx);
	gpuRenderData.staticMeshIdx	= instanceRenderData.staticMeshIdx;

	entityGPURenderDataHandle.staticMeshGPUInstanceData = m_staticMeshInstances.AddInstance(gpuRenderData);
}

void StaticMeshPrimitivesSystem::OnStaticMeshDestryed(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	SPT_PROFILER_FUNCTION();

	StaticMeshRenderDataHandle entityRenderData = registry.get<StaticMeshRenderDataHandle>(entity);
	SPT_CHECK(entityRenderData.staticMeshGPUInstanceData.IsValid());
	
	m_staticMeshInstances.RemoveInstance(entityRenderData.staticMeshGPUInstanceData);
	registry.remove<StaticMeshRenderDataHandle>(entity);
}

} // spt::rsc
