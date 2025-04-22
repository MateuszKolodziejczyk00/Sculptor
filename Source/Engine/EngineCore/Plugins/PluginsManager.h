#pragma once

#include "EngineCoreMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::engn
{

class Plugin;
class FrameContext;


class ENGINE_CORE_API PluginsManager
{
public:

	static PluginsManager& GetInstance();

	void RegisterPlugin(Plugin& plugin);

	void PostEngineInit();

	void BeginFrame(FrameContext& frameContext);


private:

	PluginsManager() = default;

	lib::DynamicArray<Plugin*> m_plugins;
};

} // spt::engn