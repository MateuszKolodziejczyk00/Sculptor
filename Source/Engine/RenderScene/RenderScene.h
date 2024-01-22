#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "RenderSceneRegistry.h"
#include "SceneRenderSystem.h"
#include "RenderSceneSubsystem.h"
#include "RenderSystemsRegistry.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"

namespace spt::rsc
{

class RenderScene;


struct TransformComponent
{
	TransformComponent();
	TransformComponent(const math::Affine3f& transform);

	void SetTransform(const math::Affine3f& newTransform);
	const math::Affine3f& GetTransform() const;

	Real32 GetUniformScale() const;

private:

	math::Affine3f m_transform;
	Real32 m_uniformScale;
};


struct RenderInstanceData
{
	TransformComponent transformComp;
};


// we don't need 64 bytes for now, but we for sure will need more than 32, so 64 will be fine to properly align suballocations
BEGIN_ALIGNED_SHADER_STRUCT(64, RenderEntityGPUData)
	SHADER_STRUCT_FIELD(math::Matrix4f, transform)
	SHADER_STRUCT_FIELD(Real32, uniformScale)
	/* 12 empty bytes */
	SHADER_STRUCT_FIELD(math::Vector4f, padding0)
	SHADER_STRUCT_FIELD(math::Vector4f, padding1)
	SHADER_STRUCT_FIELD(math::Vector4f, padding2)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(GPUSceneFrameData)
	SHADER_STRUCT_FIELD(Real32, time)
	SHADER_STRUCT_FIELD(Real32, deltaTime)
	SHADER_STRUCT_FIELD(Uint32, frameIdx)
END_SHADER_STRUCT();


DS_BEGIN(RenderSceneDS, rg::RGDescriptorSetState<RenderSceneDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<RenderEntityGPUData>),    u_renderEntitiesData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBindingDynamic<GPUSceneFrameData>), u_gpuSceneFrameConstants)
DS_END();


struct EntityGPUDataHandle
{
	EntityGPUDataHandle() = default;

	explicit EntityGPUDataHandle(const rhi::RHIVirtualAllocation& inSuballocation)
		: transformSuballocation(inSuballocation)
	{ }

	Uint32 GetEntityIdx() const
	{
		return static_cast<Uint32>(transformSuballocation.GetOffset() / sizeof(RenderEntityGPUData));
	}

	rhi::RHIVirtualAllocation transformSuballocation;
};


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

	void Update();

	// Entities =============================================================

	RenderSceneEntityHandle CreateEntity();
	RenderSceneEntityHandle CreateEntity(const RenderInstanceData& instanceData);
	void DestroyEntity(RenderSceneEntityHandle entity);

	Uint32 GetEntityIdx(RenderSceneEntityHandle entity) const;

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
	void AddRenderSystem(TArgs&&... args)
	{
		SceneRenderSystem* addedSystem = m_renderSystems.AddRenderSystem<TSystemType>(std::forward<TArgs>(args)...);
		if (addedSystem)
		{
			InitializeRenderSystem(*addedSystem);
		}
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
};

} // spt::rsc
