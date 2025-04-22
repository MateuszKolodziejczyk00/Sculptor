#include "Plugin.h"
#include "PluginsManager.h"


namespace spt::engn
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Plugin ========================================================================================

void Plugin::PostEngineInit()
{
	OnPostEngineInit();
}

void Plugin::BeginFrame(FrameContext& frameContext)
{
	OnBeginFrame(frameContext);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GenericPluginRegistrator ======================================================================

GenericPluginRegistrator::GenericPluginRegistrator(Plugin& plugin)
{
	PluginsManager::GetInstance().RegisterPlugin(plugin);
}

} // spt::engn
