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
	sceneRegistry.on_construct<StaticMeshRenderData>().connect<&StaticMeshPrimitivesSystem::OnStaticMeshUpdated>(this);
	sceneRegistry.on_update<StaticMeshRenderData>().connect<&StaticMeshPrimitivesSystem::OnStaticMeshUpdated>(this);
	sceneRegistry.on_destroy<StaticMeshRenderData>().connect<&StaticMeshPrimitivesSystem::OnStaticMeshDestryed>(this);
}

StaticMeshPrimitivesSystem::~StaticMeshPrimitivesSystem()
{
	RenderScene& owningScene = GetOwningScene();
	ecs::Registry& sceneRegistry = owningScene.GetRegistry();
	sceneRegistry.on_construct<StaticMeshRenderData>().disconnect<&StaticMeshPrimitivesSystem::OnStaticMeshUpdated>(this);
	sceneRegistry.on_update<StaticMeshRenderData>().disconnect<&StaticMeshPrimitivesSystem::OnStaticMeshUpdated>(this);
	sceneRegistry.on_destroy<StaticMeshRenderData>().disconnect<&StaticMeshPrimitivesSystem::OnStaticMeshDestryed>(this);
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

	const StaticMeshRenderData& renderData = registry.get<StaticMeshRenderData>(entity);
	
	StaticMeshRenderDataHandle& entityRenderData = registry.get_or_emplace<StaticMeshRenderDataHandle>(entity);

	if (entityRenderData.basePassInstanceData.IsValid())
	{
		// Destroy old data
		m_staticMeshInstances.RemoveInstance(entityRenderData.basePassInstanceData);
	}

	const EntityTransformHandle& transformHandle = registry.get<EntityTransformHandle>(entity);
	const Uint64 transformIdx = transformHandle.transformSuballocation.GetOffset() / sizeof(math::Affine3f);

	StaticMeshGPURenderData gpuRenderData;
	gpuRenderData.firstPrimitiveIdx	= renderData.firstPrimitiveIdx;
	gpuRenderData.primitivesNum		= renderData.primitivesNum;
	gpuRenderData.transformIdx		= static_cast<Uint32>(transformIdx);

	entityRenderData.basePassInstanceData = m_staticMeshInstances.AddInstance(gpuRenderData);
}

void StaticMeshPrimitivesSystem::OnStaticMeshDestryed(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	SPT_PROFILER_FUNCTION();

	StaticMeshRenderDataHandle entityRenderData = registry.get<StaticMeshRenderDataHandle>(entity);
	SPT_CHECK(entityRenderData.basePassInstanceData.IsValid());
	
	m_staticMeshInstances.RemoveInstance(entityRenderData.basePassInstanceData);
	registry.remove<StaticMeshRenderDataHandle>(entity);
}

} // spt::rsc
