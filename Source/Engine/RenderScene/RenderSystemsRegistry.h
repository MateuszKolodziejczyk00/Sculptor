#pragma once

#include "SculptorCoreTypes.h"
#include "SculptorECS.h"


namespace spt::rsc
{

template<typename TSystemBaseType>
class RenderSystemsRegistry
{
public:

	RenderSystemsRegistry() = default;

	const lib::DynamicArray<lib::SharedRef<TSystemBaseType>>& GetRenderSystems() const
	{
		return m_renderSystems;
	}

	template<typename TSystemType, typename... TArgs>
	TSystemBaseType* AddRenderSystem(TArgs&&... args)
	{
		SPT_PROFILER_FUNCTION();

		const RenderSystemTypeID typeID = ecs::type_id<TSystemType>();

		TSystemBaseType* addedSystem = nullptr;
		{
			const lib::LockGuard lockGuard(m_lock);

			lib::SharedPtr<TSystemBaseType>& foundSystem = m_idToRenderSystem[typeID];
			if(!foundSystem)
			{
				foundSystem = m_renderSystems.emplace_back(lib::MakeShared<TSystemType>(std::forward<TArgs>(args)...));
			}

			addedSystem = foundSystem.get();
		}
		
		return addedSystem;
	}

	template<typename TSystemType>
	lib::SharedPtr<TSystemBaseType> RemoveRenderSystem()
	{
		SPT_PROFILER_FUNCTION();

		const RenderSystemTypeID typeID = ecs::type_id<TSystemType>();
		
		lib::SharedPtr<TSystemBaseType> removedSystem;
		{
			const lib::LockGuard lockGuard(m_lock);

			const auto foundSystem = m_idToRenderSystem.find(typeID);
			if (foundSystem != std::cend(m_idToRenderSystem))
			{
				const auto foundSystemInstance = std::find_if(std::cbegin(m_renderSystems), std::cend(m_renderSystems),
															  [&foundSystem](const lib::SharedPtr<TSystemBaseType>& system)
															  {
																 return system == foundSystem->second;
															 });
				SPT_CHECK(foundSystemInstance != std::cend(m_renderSystems));
				const SizeType systemIdx = std::distance(std::cbegin(m_renderSystems), foundSystemInstance);
				removedSystem = m_renderSystems[systemIdx];

				m_renderSystems.erase(std::begin(m_renderSystems) + systemIdx);
				m_idToRenderSystem.erase(foundSystem);
			}
		}

		return removedSystem;
	}

	template<typename TSystemType>
	lib::SharedPtr<TSystemType> FindRenderSystem() const
	{
		const lib::LockGuard lockGuard(m_lock);
		
		const auto foundSystem = m_idToRenderSystem.find(ecs::type_id<TSystemType>());

		return foundSystem != std::cend(m_idToRenderSystem) ? std::static_pointer_cast<TSystemType>(foundSystem->second) : nullptr;
	}

	template<typename TCallable>
	void ForEachSystem(TCallable&& callable)
	{
		const lib::LockGuard lockGuard(m_lock);

		for (const lib::SharedRef<TSystemBaseType>& system : m_renderSystems)
		{
			callable(*system);
		}
	}

private:

	using RenderSystemTypeID = ecs::type_info;

	mutable lib::Lock m_lock;

	lib::DynamicArray<lib::SharedRef<TSystemBaseType>>                m_renderSystems;
	lib::HashMap<RenderSystemTypeID, lib::SharedPtr<TSystemBaseType>> m_idToRenderSystem;
};

} // spt::rsc