#include "ThreadContext.h"

namespace spt::tcx
{

struct ThreadContextInternal
{
	lib::MemoryArena scratchArena = lib::MemoryArena("ThreadContextScratchArena", 1024u * 1024u, 64u * 1024u * 1024u);
};


thread_local lib::TypeStorage<ThreadContextInternal> g_threadContext;


void InitThreadContext()
{
	g_threadContext.Construct();
}

void ShutdownThreadContext()
{
	g_threadContext.Destroy();
}

lib::MemoryArena& Scratch()
{
	return g_threadContext.Get().scratchArena;
}

lib::MemoryArenaScope ScratchScope()
{
	return lib::MemoryArenaScope(g_threadContext.Get().scratchArena);
}

} // spt::tcx
