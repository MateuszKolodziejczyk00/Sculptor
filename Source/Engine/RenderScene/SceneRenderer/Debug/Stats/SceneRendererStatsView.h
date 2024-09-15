#pragma once

#if SPT_ENABLE_SCENE_RENDERER_STATS

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rsc
{

class SceneRendererStatsView
{
public:

	SceneRendererStatsView()          = default;
	virtual ~SceneRendererStatsView() = default;

	virtual void DrawUI() {}
};


class RENDER_SCENE_API SceneRendererStatsRegistry
{
public:

	static SceneRendererStatsRegistry& GetInstance();

	template<typename TStatsView>
	TStatsView& GetStatsView()
	{
		constexpr lib::TypeID id = lib::TypeInfo<TStatsView>::id;

		lib::LockGuard lock(m_entriesLock);

		const auto foundEntryIt = std::find_if(m_entries.begin(), m_entries.end(), [id](const Entry& entry)
											   {
												   return entry.id == id;
											   });

		if (foundEntryIt != m_entries.end())
		{
			return *static_cast<TStatsView*>(foundEntryIt->view.get());
		}

		Entry& entry = m_entries.emplace_back();
		entry.id   = id;
		entry.name = lib::TypeInfo<TStatsView>::name;
		entry.view = lib::MakeShared<TStatsView>();

		return *static_cast<TStatsView*>(entry.view.get());
	}

	void DrawUI();

private:

	SceneRendererStatsRegistry() = default;

	struct Entry
	{
		lib::TypeID                            id;
		lib::String                            name;
		lib::SharedPtr<SceneRendererStatsView> view;
	};

	lib::DynamicArray<Entry> m_entries;

	lib::Lock m_entriesLock;
};

} // spt::rsc

#endif // SPT_ENABLE_SCENE_RENDERER_STATS