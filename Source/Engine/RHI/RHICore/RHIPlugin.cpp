#include "RHIPlugin.h"


namespace  spt::rhi
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIExtensionsProvider =========================================================================

RHIPlugin::RHIPlugin()
{
	RHIPluginsManager::RegisterPlugin(*this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Buffer ========================================================================================

RHIPluginsManager& RHIPluginsManager::Get()
{
	static RHIPluginsManager instance;
	return instance;
}

void RHIPluginsManager::RegisterPlugin(RHIPlugin& provider)
{
	Get().m_plugins.emplace_back(&provider);
}

void RHIPluginsManager::CollectExtensions()
{
	ExtensionsCollector collector(m_rhiExtensions, m_deviceExtensions);
	
	for (const RHIPlugin* plugin : m_plugins)
	{
		plugin->CollectExtensions(collector);
	}
}

} //  spt::rhi