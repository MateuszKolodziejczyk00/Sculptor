#if SPT_ENABLE_SCENE_RENDERER_STATS
#include "SceneRendererStatsView.h"
#include "ImGui/SculptorImGui.h"


namespace spt::rsc
{

SceneRendererStatsRegistry& SceneRendererStatsRegistry::GetInstance()
{
	static SceneRendererStatsRegistry instance;
	return instance;
}

void SceneRendererStatsRegistry::DrawUI(void* ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));

	for (const Entry& entry : m_entries)
	{
		if (ImGui::CollapsingHeader(entry.name.c_str()))
		{
			entry.view->DrawUI();
		}
	}
}

} // spt::rsc

#endif // SPT_ENABLE_SCENE_RENDERER_STATS
