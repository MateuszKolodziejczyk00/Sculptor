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


struct OnExtractDataPerSceneDelegate
{
	lib::Delegate<void(RenderScene& /*scene*/)> m_callable;
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

	template<typename TSystemType>
	void AddRenderSystem()
	{
		SPT_PROFILER_FUNCTION();

		const RenderSystemTypeID typeID = ecs::type_id<TSystemType>();

		const lib::LockGuard lockGuard(m_lock);

		lib::UniquePtr<RenderSystem> system = m_renderSystems[typeID];
		if (!system)
		{
			system = std::make_unique<RenderSystem>();
			InitializeRenderSystem(*system);
		}
	}

	template<typename TSystemType>
	void RemoveRenderSystem()
	{
		SPT_PROFILER_FUNCTION();

		const RenderSystemTypeID typeID = ecs::type_id<TSystemType>();
		
		const lib::LockGuard lockGuard(m_lock);
		
		const auto foundSystem = m_renderSystems.find(typeID);
		if (foundSystem != std::cend(m_renderSystems))
		{
			DeinitializeRenderSystem(*foundSystem->second);
		}
	}

private:

	void InitializeRenderSystem(RenderSystem& system);
	void DeinitializeRenderSystem(RenderSystem& system);

	using RenderSystemTypeID = ecs::type_info;

	RenderSceneRegistry m_registry;

	lib::SharedPtr<rdr::Buffer> m_instanceTransforms;

	lib::Lock m_lock;
	lib::HashMap<RenderSystemTypeID, lib::UniquePtr<RenderSystem>> m_renderSystems;
};

} // spt::rsc
