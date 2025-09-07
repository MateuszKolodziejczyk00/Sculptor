#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderSceneRegistry.h"
#include "SceneRenderSystem.h"
#include "RenderSceneSubsystem.h"
#include "RenderSystemsRegistry.h"
#include "RenderSceneTypes.h"


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


class RenderSceneSubsystemsRegistry
{
public:

	RenderSceneSubsystemsRegistry() = default;

	const lib::DynamicArray<lib::SharedPtr<RenderSceneSubsystem>>& GetSystems() const
	{
		return m_systems;
	}

	template<typename TPrimitiveSystem>
	lib::SharedPtr<TPrimitiveSystem> GetSystem() const
	{
		const PrimitiveSystemTypeID typeID = ecs::type_id<TPrimitiveSystem>();
		const auto foundSystem = m_typeIDToSystem.find(typeID);
		return foundSystem != std::cend(m_typeIDToSystem) ?  std::static_pointer_cast<TPrimitiveSystem>(foundSystem->second) : nullptr;
	}

	template<typename TPrimitiveSystem, typename... TArgs>
	void AddSystem(RenderScene& scene, TArgs&&... args)
	{
		SPT_PROFILER_FUNCTION();

		const PrimitiveSystemTypeID typeID = ecs::type_id<TPrimitiveSystem>();

		const lib::LockGuard lockGuard(m_lock);

		lib::SharedPtr<RenderSceneSubsystem>& system = m_typeIDToSystem[typeID];
		if (!system)
		{
			const lib::SharedPtr<TPrimitiveSystem> systemInstance = lib::MakeShared<TPrimitiveSystem>(scene, std::forward<TArgs>(args)...);
			m_systems.emplace_back(systemInstance);
			system = m_systems.back();
		}
	}

	template<typename TPrimitiveSystem>
	void RemoveSystem()
	{
		SPT_PROFILER_FUNCTION();

		const PrimitiveSystemTypeID typeID = ecs::type_id<TPrimitiveSystem>();

		const lib::LockGuard lockGuard(m_lock);
	
		const auto foundSystem = m_typeIDToSystem.find(typeID);
		if (foundSystem != std::cend(m_typeIDToSystem))
		{
			const lib::SharedPtr<RenderSceneSubsystem> systemToRemove = foundSystem->second;
			const auto systemInstanceIt = std::find_if(std::cbegin(m_systems), std::cend(m_systems),
													   [&systemToRemove](const lib::SharedPtr<RenderSceneSubsystem>& system)
													   {
														   return system == systemToRemove;
													   });

			SPT_CHECK(systemInstanceIt != std::cend(m_systems));
			m_systems.erase(systemInstanceIt);
			m_typeIDToSystem.erase(foundSystem);
		}
	}

private:

	using PrimitiveSystemTypeID = ecs::type_info;

	lib::Lock m_lock;

	// Store both containers to allow fast iterations during update and fast lookup by type for getting systems
	lib::DynamicArray<lib::SharedPtr<RenderSceneSubsystem>> m_systems;
	lib::HashMap<PrimitiveSystemTypeID, lib::SharedPtr<RenderSceneSubsystem>> m_typeIDToSystem;
};


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
		SceneRenderSystem* addedSystem = m_renderSystems.AddRenderSystem<TSystemType>(std::forward<TArgs>(args)...);
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
	
	// Render Scene Subsystems ==============================================

	template<typename TSubsystem, typename... TArgs>
	void AddSceneSubsystem(TArgs&&... args)
	{
		m_renderSceneSubsystems.AddSystem<TSubsystem>(*this, std::forward<TArgs>(args)...);
	}

	template<typename TSubsystem>
	void RemoveSceneSubsystem()
	{
		m_renderSceneSubsystems.RemoveSystem<TSubsystem>();
	}

	template<typename TSubsystem>
	lib::SharedPtr<TSubsystem> GetSceneSubsystem() const
	{
		return m_renderSceneSubsystems.GetSystem<TSubsystem>();
	}
	
	template<typename TSubsystem>
	TSubsystem& GetSceneSubsystemChecked() const
	{
		const lib::SharedPtr<TSubsystem> subsystem = GetSceneSubsystem<TSubsystem>();
		SPT_CHECK(!!subsystem);
		return *subsystem;
	}

private:

	lib::SharedRef<rdr::Buffer> CreateInstancesBuffer() const;
	lib::MTHandle<RenderSceneDS> CreateRenderSceneDS() const;

	void InitializeRenderSystem(SceneRenderSystem& system);
	void DeinitializeRenderSystem(SceneRenderSystem& system);

	RenderSceneRegistry m_registry;

	lib::SharedRef<rdr::Buffer> m_renderEntitiesBuffer;

	lib::MTHandle<RenderSceneDS> m_renderSceneDS;

	RenderSceneSubsystemsRegistry m_renderSceneSubsystems;

	RenderSystemsRegistry<SceneRenderSystem> m_renderSystems;

	lib::SharedPtr<const engn::FrameContext> m_currentFrame;
};

} // spt::rsc
