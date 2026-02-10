#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderSceneRegistry.h"
#include "SceneRenderSystem.h"
#include "RenderSystemsRegistry.h"
#include "RenderSceneConstants.h"


namespace spt::engn
{
class FrameContext;
} // spt::engn


namespace spt::rsc
{

struct EntityGPUDataHandle
{
	EntityGPUDataHandle() = default;

	explicit EntityGPUDataHandle(const rhi::RHIVirtualAllocation& inSuballocation)
		: transformSuballocation(inSuballocation)
	{ }

	RenderEntityGPUPtr GetGPUDataPtr() const
	{
		return RenderEntityGPUPtr(static_cast<Uint32>(transformSuballocation.GetOffset()));
	}

	rhi::RHIVirtualAllocation transformSuballocation;
};
SPT_REGISTER_COMPONENT_TYPE(EntityGPUDataHandle, RenderSceneRegistry);


class RENDER_SCENE_API RenderScene
{
public:

	RenderScene();

	RenderSceneRegistry& GetRegistry();
	const RenderSceneRegistry& GetRegistry() const;

	void BeginFrame(const engn::FrameContext& frame);
	void EndFrame();

	void Update();

	const engn::FrameContext& GetCurrentFrameRef() const;

	// Entities =============================================================

	RenderSceneEntityHandle CreateEntity();
	RenderSceneEntityHandle CreateEntity(const RenderInstanceData& instanceData);
	void DestroyEntity(RenderSceneEntityHandle entity);

	// Rendering ============================================================

	const lib::SharedRef<rdr::Buffer>& GetRenderEntitiesBuffer() const;

	const lib::MTHandle<RenderSceneDS>& GetRenderSceneDS() const;
	
	// Render Systems =======================================================

	const lib::DynamicArray<lib::SharedRef<SceneRenderSystem>>& GetRenderSystems() const;

	template<typename TSystemType>
	lib::SharedPtr<TSystemType> FindRenderSystem() const
	{
		return m_renderSystems.FindRenderSystem<TSystemType>();
	}
	
	template<typename TSystemType>
	TSystemType& GetRenderSystemChecked() const
	{
		const lib::SharedPtr<TSystemType> renderSystem = m_renderSystems.FindRenderSystem<TSystemType>();
		SPT_CHECK(!!renderSystem);
		return *renderSystem;
	}

	template<typename TSystemType, typename... TArgs>
	TSystemType* AddRenderSystem(TArgs&&... args)
	{
		SceneRenderSystem* addedSystem = m_renderSystems.AddRenderSystem<TSystemType>(*this, std::forward<TArgs>(args)...);
		if (addedSystem)
		{
			InitializeRenderSystem(*addedSystem);
		}

		return static_cast<TSystemType*>(addedSystem);
	}

	template<typename TSystemType>
	void RemoveRenderSystem()
	{
		lib::SharedPtr<SceneRenderSystem> removedSystem = m_renderSystems.RemoveRenderSystem<TSystemType>();
		if (removedSystem)
		{
			DeinitializeRenderSystem(*removedSystem);
		}
	}

private:

	lib::SharedRef<rdr::Buffer> CreateInstancesBuffer() const;
	lib::MTHandle<RenderSceneDS> CreateRenderSceneDS() const;

	void InitializeRenderSystem(SceneRenderSystem& system);
	void DeinitializeRenderSystem(SceneRenderSystem& system);

	RenderSceneRegistry m_registry;

	lib::SharedRef<rdr::Buffer> m_renderEntitiesBuffer;

	lib::MTHandle<RenderSceneDS> m_renderSceneDS;

	RenderSystemsRegistry<SceneRenderSystem> m_renderSystems;

	lib::SharedPtr<const engn::FrameContext> m_currentFrame;
};

} // spt::rsc
