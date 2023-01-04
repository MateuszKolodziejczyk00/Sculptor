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
	sceneRegistry.emplace<BasePassStaticMeshRenderDataHandle>(test);

	sceneRegistry.on_construct<BasePassStaticMeshRenderDataHandle>().connect<&StaticMeshRenderSystem::PostBasePassSMConstructed>(this);
	sceneRegistry.on_destroy<BasePassStaticMeshRenderDataHandle>().connect<&StaticMeshRenderSystem::PreBasePassSMDestroyed>(this);

	OnExtractDataPerSceneDelegate& onExtractDataPerScene = GetSystemEntity().emplace<OnExtractDataPerSceneDelegate>();
	onExtractDataPerScene.m_callable.BindRawMember(this, &StaticMeshRenderSystem::ExtractDataPerScene);
}

void StaticMeshRenderSystem::ExtractDataPerScene(const RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	m_basePassInstances.FlushRemovedInstances();
}

void StaticMeshRenderSystem::PostBasePassSMConstructed(ecs::Registry& registry, ecs::Entity entity)
{
	SPT_PROFILER_FUNCTION();

	BasePassStaticMeshRenderDataHandle& entityRenderData = registry.get<BasePassStaticMeshRenderDataHandle>(entity);
	SPT_CHECK(!entityRenderData.basePassInstanceData.IsValid());

	BasePassStaticMeshRenderData renderData = registry.ctx().get<BasePassStaticMeshRenderData>();
	entityRenderData.basePassInstanceData = m_basePassInstances.AddInstance(renderData);
}

void StaticMeshRenderSystem::PreBasePassSMDestroyed(ecs::Registry& registry, ecs::Entity entity)
{
	SPT_PROFILER_FUNCTION();

	BasePassStaticMeshRenderDataHandle& entityRenderData = registry.get<BasePassStaticMeshRenderDataHandle>(entity);
	SPT_CHECK(entityRenderData.basePassInstanceData.IsValid());
	
	m_basePassInstances.RemoveInstance(entityRenderData.basePassInstanceData);
}

} // spt::rsc
