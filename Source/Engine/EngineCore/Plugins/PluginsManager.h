#pragma once

#include "EngineCoreMacros.h"
#include "Plugins/Plugin.h"
#include "SculptorCoreTypes.h"
#include "Utility/Templates/Callable.h"


namespace spt::engn
{

class Plugin;
class FrameContext;


struct PluginFactory
{
	lib::RuntimeTypeInfo pluginType;
	PluginFactoryFunction factory;
};


class ENGINE_CORE_API PluginFactoriesRegistry
{
public:

	static PluginFactoriesRegistry& GetInstance();

	void RegisterFactory(const PluginFactory& factory);

	const lib::DynamicArray<PluginFactory>& GetFactories() const;

private:

	lib::DynamicArray<PluginFactory> m_factories;
};


class ENGINE_CORE_API PluginsManager
{
public:

	PluginsManager() = default;

	void InitializePlugins(const lib::Span<const PluginFactory>& pluginFactories);

	template<typename TPluginType>
	TPluginType* GetPlugin() const
	{
		return static_cast<TPluginType*>(GetPlugin(lib::TypeInfo<TPluginType>()));
	}

	template<typename TPluginType>
	TPluginType& GetPluginChecked() const
	{
		TPluginType* plugin = GetPlugin<TPluginType>();
		SPT_CHECK(!!plugin);
		return *plugin;
	}

	Plugin* GetPlugin(const lib::RuntimeTypeInfo& pluginType) const;

	void BeginFrame(FrameContext& frameContext);

private:

	struct PluginEntry
	{
		lib::UniquePtr<Plugin> plugin;
		lib::RuntimeTypeInfo   pluginType;
	};

	lib::DynamicArray<PluginEntry> m_plugins;
};

} // spt::engn
