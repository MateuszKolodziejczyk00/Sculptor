#include "RendererSettings.h"
#include "YAMLSerializerHelper.h"
#include "ConfigUtils.h"

namespace spt::srl
{

template<>
struct TypeSerializer<rdr::RendererSettings>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& settings)
	{
		serializer.Serialize("FramesInFlightNum", settings.framesInFlight);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::rdr::RendererSettings)

namespace spt::rdr
{

struct StaticRendererSettings
{
	StaticRendererSettings()
	{
		engn::ConfigUtils::LoadConfigData(settings, "RendererSettings.yaml");
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
