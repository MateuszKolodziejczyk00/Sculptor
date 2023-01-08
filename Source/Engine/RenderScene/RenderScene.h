#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "RenderSceneRegistry.h"
#include "RenderSystem.h"
#include "PrimitivesSystem.h"

namespace spt::rsc
{

class RenderScene;


struct RenderInstanceData
{
	math::Affine3f transfrom;
};


struct EntityTransformHandle
{
	EntityTransformHandle() = default;

	explicit EntityTransformHandle(const rhi::RHISuballocation& inSuballocation)
		: transformSuballocation(inSuballocation)
	{ }

	rhi::RHISuballocation transformSuballocation;
};


class RenderSystemsRegistry
{
public:

	RenderSystemsRegistry() = default;

	const lib::DynamicArray<lib::UniquePtr<RenderSystem>>& GetRenderSystems() const
	{
		return m_renderSystems;
	}

	template<typename TSystemType>
	RenderSystem* AddRenderSystem()
	{
		SPT_PROFILER_FUNCTION();

		const RenderSystemTypeID typeID = ecs::type_id<TSystemType>();

		RenderSystem* addedSystem = nullptr;
		{
			const lib::LockGuard lockGuard(m_lock);

			if (std::find(std::cbegin(m_renderSystemsID), std::cend(m_renderSystemsID), typeID) == std::cend(m_renderSystemsID))
			{
				addedSystem = m_renderSystems.emplace_back(std::make_unique<RenderSystem>()).get();
				m_renderSystemsID.emplace_back(typeID);
			}
		}
		
		return addedSystem;
	}

	template<typename TSystemType>
	lib::UniquePtr<RenderSystem> RemoveRenderSystem()
	{
		SPT_PROFILER_FUNCTION();

		const RenderSystemTypeID typeID = ecs::type_id<TSystemType>();
		
		lib::UniquePtr<RenderSystem> removedSystem;
		{
			const lib::LockGuard lockGuard(m_lock);

			const auto foundSystem = std::find(std::cbegin(m_renderSystemsID), std::cend(m_renderSystemsID), typeID);
			if (foundSystem != std::cend(m_renderSystems))
			{
				const SizeType systemIdx = std::distance(std::cbegin(m_renderSystemsID), foundSystem);
				removedSystem = std::move(m_renderSystems[systemIdx]);

				m_renderSystems.erase(std::begin(m_renderSystems) + systemIdx);
				m_renderSystemsID.erase(std::begin(m_renderSystemsID) + systemIdx);
			}
		}

		return removedSystem;
	}

private:

	using RenderSystemTypeID = ecs::type_info;

	lib::Lock m_lock;
	// These to arrays must match - m_systemsID[i] is id of system m_renderSystems[i]
	lib::DynamicArray<lib::UniquePtr<RenderSystem>> m_renderSystems;
	lib::DynamicArray<RenderSystemTypeID>			m_renderSystemsID;
};


class PrimitiveSystemsRegistry
{
public:

	PrimitiveSystemsRegistry() = default;

	const lib::DynamicArray<lib::UniquePtr<PrimitivesSystem>>& GetSystems() const
	{
		return m_systems;
	}

	template<typename TPrimitiveSystem>
	TPrimitiveSystem* GetSystem() const
	{
		const PrimitiveSystemTypeID typeID = ecs::type_id<TPrimitiveSystem>();
		const auto foundSystem = m_typeIDToSystem.find(typeID);
		return foundSystem != std::cend(m_typeIDToSystem) ? foundSystem->second : nullptr;
	}


	template<typename TPrimitiveSystem>
	void AddSystem(RenderScene& scene)
	{
		SPT_PROFILER_FUNCTION();

		const PrimitiveSystemTypeID typeID = ecs::type_id<TPrimitiveSystem>();

		const lib::LockGuard lockGuard(m_lock);

		PrimitivesSystem*& system = m_typeIDToSystem[typeID];
		if (!system)
		{
			m_systems.emplace_back(std::make_unique<TPrimitiveSystem>(scene));
			system = m_systems.back().get();
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
			const PrimitivesSystem* systemToRemove = foundSystem->second;
			const auto systemInstanceIt = std::find_if(std::cbegin(m_systems), std::cend(m_systems),
													   [systemToRemove](const lib::UniquePtr<PrimitivesSystem>& system)
													   {
														   return system.get() == systemToRemove;
													   });

			SPT_CHECK(systemInstanceIt != std::cend(m_systems));
			m_systems.erase(systemInstanceIt);
			m_typeIDToSystem.erase(foundSystem);
		}
	}

private:

	using PrimitiveSystemTypeID = ecs::type_info;

	lib::Lock m_lock;

	lib::DynamicArray<lib::UniquePtr<PrimitivesSystem>> m_systems;
	lib::HashMap<PrimitiveSystemTypeID, PrimitivesSystem*> m_typeIDToSystem;
};


class RENDER_SCENE_API RenderScene
{
public:

	RenderScene();

	ecs::Registry& GetRegistry();
	const ecs::Registry& GetRegistry() const;

	void Update();

	// Entities =============================================================

	RenderSceneEntityHandle CreateEntity(const RenderInstanceData& instanceData);
	void DestroyEntity(RenderSceneEntityHandle entity);

	Uint64 GetTransformIdx(RenderSceneEntityHandle entity) const;
	
	// Render Systems =======================================================

	const lib::DynamicArray<lib::UniquePtr<RenderSystem>>& GetRenderSystems() const;

	template<typename TSystemType>
	void AddRenderSystem()
	{
		RenderSystem* addedSystem = m_renderSystems.AddRenderSystem<TSystemType>();
		if (addedSystem)
		{
			InitializeRenderSystem(*addedSystem);
		}
	}

	template<typename TSystemType>
	void RemoveRenderSystem()
	{
		lib::UniquePtr<RenderSystem> removedSystem = m_renderSystems.RemoveRenderSystem<TSystemType>();
		if (removedSystem)
		{
			DeinitializeRenderSystem(*removedSystem);
		}
	}
	
	// Primitives Systems ===================================================

	template<typename TPrimitivesSystem>
	void AddPrimitivesSystem()
	{
		m_primitiveSystems.AddSystem<TPrimitivesSystem>(*this);
	}

	template<typename TPrimitivesSystem>
	void RemovePrimitivesSystem()
	{
		m_primitiveSystems.RemoveSystem<TPrimitivesSystem>();
	}

	template<typename TPrimitivesSystem>
	const TPrimitivesSystem* GetPrimitivesSystem() const
	{
		return m_primitiveSystems.GetSystem<TPrimitivesSystem>();
	}
	
	template<typename TPrimitivesSystem>
	const TPrimitivesSystem& GetPrimitivesSystemChecked() const
	{
		const TPrimitivesSystem* system = GetPrimitivesSystem<TPrimitivesSystem>();
		SPT_CHECK(!!system);
		return *system;
	}

private:

	void InitializeRenderSystem(RenderSystem& system);
	void DeinitializeRenderSystem(RenderSystem& system);

	RenderSceneRegistry m_registry;

	lib::SharedPtr<rdr::Buffer> m_instanceTransforms;

	PrimitiveSystemsRegistry m_primitiveSystems;

	RenderSystemsRegistry m_renderSystems;
};

} // spt::rsc
