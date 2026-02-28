#pragma once

#include "EngineCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Utility/Templates/Callable.h"


namespace spt::engn
{

class FrameContext;


class ENGINE_CORE_API Plugin
{
public:

	void PostEngineInit();

	void BeginFrame(FrameContext& frameContext);

protected:

	Plugin() = default;

	virtual void OnPostEngineInit() {}
	virtual void OnBeginFrame(FrameContext& frameContext) {}
};


using PluginFactoryFunction = lib::RawCallable<lib::UniquePtr<Plugin>()>;


struct ENGINE_CORE_API GenericPluginRegistrator
{
	GenericPluginRegistrator(const lib::RuntimeTypeInfo& type, PluginFactoryFunction factory);
};


template<typename TPlugin>
struct PluginRegistrator : public GenericPluginRegistrator
{
	PluginRegistrator()
		: GenericPluginRegistrator(lib::TypeInfo<TPlugin>(), &PluginRegistrator::CreatePlugin)
	{
	}

	static lib::UniquePtr<Plugin> CreatePlugin()
	{
		return std::make_unique<TPlugin>();
	}
};

} // spt::engn


#define SPT_DEFINE_PLUGIN(Type) \
engn::PluginRegistrator<Type> g_pluginRegistrator;
