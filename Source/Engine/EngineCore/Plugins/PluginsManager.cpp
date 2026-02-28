#include "PluginsManager.h"
#include "Plugin.h"


namespace spt::engn
{

SPT_DEFINE_LOG_CATEGORY(LogPluginsManager, true);

//////////////////////////////////////////////////////////////////////////////////////////////////
// PluginFactoriesRegistry =======================================================================

PluginFactoriesRegistry& PluginFactoriesRegistry::GetInstance()
{
	static PluginFactoriesRegistry instance;
	return instance;
}

void PluginFactoriesRegistry::RegisterFactory(const PluginFactory& factory)
{
	const Bool containsPlugin = lib::ContainsPred(m_factories, [&factory](const PluginFactory& registeredFactory)
	{
		return registeredFactory.pluginType == factory.pluginType;
	});

	if (!containsPlugin)
	{
		m_factories.emplace_back(factory);
	}
}

const lib::DynamicArray<PluginFactory>& PluginFactoriesRegistry::GetFactories() const
{
	return m_factories;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// PluginsManager=================================================================================

void PluginsManager::InitializePlugins(const lib::Span<const PluginFactory>& pluginFactories)
{
	for (const PluginFactory& factory : pluginFactories)
	{
		const lib::RuntimeTypeInfo pluginTypeInfo = factory.pluginType;
		const Bool alreadyRegistered = lib::ContainsPred(m_plugins, [&pluginTypeInfo](const PluginEntry& entry)
		{
			return entry.pluginType == pluginTypeInfo;
		});

		if (!alreadyRegistered)
		{
			std::unique_ptr<Plugin> plugin = factory.factory();

			SPT_LOG_INFO(LogPluginsManager, "Plugin registered: {}", pluginTypeInfo.name.data());
			const PluginEntry& newEntry = m_plugins.emplace_back(PluginEntry{ std::move(plugin), pluginTypeInfo });

			newEntry.plugin->PostEngineInit();
		}
	}
}

Plugin* PluginsManager::GetPlugin(const lib::RuntimeTypeInfo& pluginType) const
{
	const auto foundIt = std::find_if(m_plugins.begin(), m_plugins.end(), [&pluginType](const PluginEntry& entry)
	{
		return entry.pluginType == pluginType;
	});

	return foundIt != m_plugins.end() ? foundIt->plugin.get() : nullptr;
}

void PluginsManager::BeginFrame(FrameContext& frameContext)
{
	SPT_PROFILER_FUNCTION();

	for (const PluginEntry& pluginEntry : m_plugins)
	{
		pluginEntry.plugin->BeginFrame(frameContext);
	}
}

} // spt::engn
