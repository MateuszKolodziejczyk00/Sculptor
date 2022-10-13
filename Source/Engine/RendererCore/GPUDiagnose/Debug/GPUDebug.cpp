#include "GPUDebug.h"
#include "CommandsRecorder/CommandsRecorder.h"

namespace spt::rdr
{

DebugRegion::DebugRegion(CommandsRecorder& recorder, const lib::HashedString& regionName, const lib::Color& color)
	: m_cachedRecorder(recorder)
{
	SPT_CHECK(m_cachedRecorder.IsBuildingCommands());
	m_cachedRecorder.BeginDebugRegion(regionName, color);
}

DebugRegion::~DebugRegion()
{
	SPT_CHECK(m_cachedRecorder.IsBuildingCommands());
	m_cachedRecorder.EndDebugRegion();
}

} // spt::rdr
