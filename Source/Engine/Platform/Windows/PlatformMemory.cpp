#include "PlatformMemory.h"
#include "Windows.h"


namespace spt::mem
{

SizeType GetPageSize()
{
	return 4096u;
}

Byte* ReserveVirtualMemory(SizeType size)
{
	return reinterpret_cast<Byte*>(VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE));
}

void ReleaseVirtualMemory(Byte* address, SizeType size)
{
	VirtualFree(address, 0, MEM_RELEASE);
}

void CommitVirtualMemory(Byte* address, SizeType size)
{
	VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE);
}

void DecommitVirtualMemory(Byte* address, SizeType size)
{
	VirtualFree(address, size, MEM_DECOMMIT);
}

} // spt::mem
