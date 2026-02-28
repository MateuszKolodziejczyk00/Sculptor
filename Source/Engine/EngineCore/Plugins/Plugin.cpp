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

GenericPluginRegistrator::GenericPluginRegistrator(const lib::RuntimeTypeInfo& type, PluginFactoryFunction factory)
{
	PluginFactory pluginFactory;
	pluginFactory.pluginType = type;
	pluginFactory.factory    = factory;
	PluginFactoriesRegistry::GetInstance().RegisterFactory(pluginFactory);
}

} // spt::engn
