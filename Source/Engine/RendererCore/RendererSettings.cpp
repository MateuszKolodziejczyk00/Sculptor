#include "RendererSettings.h"
#include "ConfigUtils.h"


namespace spt::rdr
{

struct StaticRendererSettings
{
	StaticRendererSettings()
	{
		engn::ConfigUtils::LoadConfigData(settings, "RendererSettings.json");
	}

	RendererSettings settings;
};

RendererSettings::RendererSettings()
	: framesInFlight(2)
{ }

const RendererSettings& RendererSettings::Get()
{
	static StaticRendererSettings instance;
	return instance.settings;
}

} // spt::rdr
