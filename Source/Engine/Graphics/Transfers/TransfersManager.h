#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Semaphore.h"


namespace spt::rdr
{
class CommandRecorder;
class SemaphoresArray;
class RenderContext;
} // spt::rdr


namespace spt::gfx
{

class GRAPHICS_API TransfersManager
{
public:

	static TransfersManager& Get();

	static void WaitForTransfersFinished();
	static void WaitForTransfersFinished(Uint64 transferSignalValue);
	static void WaitForTransfersFinished(rdr::SemaphoresArray& waitSemaphoresArray);

	static Uint64 RecordAndSubmitTransfers(const lib::SharedRef<rdr::RenderContext>& context, lib::UniquePtr<rdr::CommandRecorder> recorder);

private:

	TransfersManager();

	lib::Lock                      m_lock;
	lib::SharedPtr<rdr::Semaphore> m_transferFinishedSemaphore;
	std::atomic<Uint64>            m_lastSubmitCountValue;
};

} // spt::gfx