#pragma once

#include "EngineCoreMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::engn
{

class FrameContext;


class ENGINE_CORE_API Plugin
{
public:

	virtual const char* GetName() const = 0;

	void PostEngineInit();

	void BeginFrame(FrameContext& frameContext);

protected:

	Plugin() = default;

	virtual void OnPostEngineInit() {}
	virtual void OnBeginFrame(FrameContext& frameContext) {}
};



struct ENGINE_CORE_API GenericPluginRegistrator
{
	GenericPluginRegistrator(Plugin& plugin);
};


template<typename TPlugin>
struct PluginRegistrator : public GenericPluginRegistrator
{
	PluginRegistrator()
		: GenericPluginRegistrator(TPlugin::Get())
	{
	}
};

} // spt::engn


#define SPT_GENERATE_PLUGIN(Type) \
private: \
	Type() = default; \
public: \
	static Type& Get(); \
	const char* GetName() const override { return #Type; }


#define SPT_DEFINE_PLUGIN(Type) \
Type& Type::Get() \
{ \
	static Type instance; \
	return instance; \
} \
engn::PluginRegistrator<Type> g_pluginRegistrator;