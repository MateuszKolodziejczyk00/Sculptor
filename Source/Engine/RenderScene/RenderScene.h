#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "RenderSceneRegistry.h"
#include "RenderSystems/RenderSystem.h"

namespace spt::rsc
{

struct RenderInstanceData
{
	math::Transform3f transfrom;
};


struct EntityTransformHandle
{
	EntityTransformHandle() = default;

	explicit EntityTransformHandle(const rhi::RHISuballocation& inSuballocation)
		: transformSuballocation(inSuballocation)
	{ }

	rhi::RHISuballocation transformSuballocation;
};


class RENDER_SCENE_API RenderScene
{
public:

	RenderScene();

	ecs::Registry& GetRegistry();
	const ecs::Registry& GetRegistry() const;

	// Entities =============================================================

	RenderSceneEntityHandle CreateEntity(const RenderInstanceData& instanceData);
	void DestroyEntity(RenderSceneEntityHandle entity);

	Uint64 GetTransformIdx(RenderSceneEntityHandle entity) const;
	
	// Render Systems =======================================================

	const lib::DynamicArray<lib::UniquePtr<RenderSystem>>& GetRenderSystems() const;

	template<typename TSystemType>
	void AddRenderSystem()
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
		
		if (addedSystem)
		{
			InitializeRenderSystem(*addedSystem);
		}
	}

	template<typename TSystemType>
	void RemoveRenderSystem()
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

		if (removedSystem)
		{
			DeinitializeRenderSystem(*removedSystem);
		}
	}

private:

	void InitializeRenderSystem(RenderSystem& system);
	void DeinitializeRenderSystem(RenderSystem& system);

	using RenderSystemTypeID = ecs::type_info;

	RenderSceneRegistry m_registry;

	lib::SharedPtr<rdr::Buffer> m_instanceTransforms;

	lib::Lock m_lock;
	// These to arrays must match - m_systemsID[i] is id of system m_renderSystems[i]
	lib::DynamicArray<lib::UniquePtr<RenderSystem>> m_renderSystems;
	lib::DynamicArray<RenderSystemTypeID>			m_renderSystemsID;
};

} // spt::rsc
