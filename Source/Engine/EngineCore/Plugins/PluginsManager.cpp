#include "PluginsManager.h"
#include "Plugin.h"


namespace spt::engn
{

SPT_DEFINE_LOG_CATEGORY(LogPluginsManager, true);


PluginsManager& PluginsManager::GetInstance()
{
	static PluginsManager instance;
	return instance;
}

void PluginsManager::RegisterPlugin(Plugin& plugin)
{
	m_plugins.push_back(&plugin);

	SPT_LOG_INFO(LogPluginsManager, "Plugin registered: {}", plugin.GetName());
}

void PluginsManager::PostEngineInit()
{
	for (Plugin* plugin : m_plugins)
	{
		plugin->PostEngineInit();
	}
}

void PluginsManager::BeginFrame(FrameContext& frameContext)
{
	SPT_PROFILER_FUNCTION();

	for (Plugin* plugin : m_plugins)
	{
		plugin->BeginFrame(frameContext);
	}
}

} // spt::engn