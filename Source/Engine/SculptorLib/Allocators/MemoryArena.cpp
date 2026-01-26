#include "MemoryArena.h"
#include "Assertions/Assertions.h"
#include "MathUtils.h"
#include "PlatformMemory.h"


namespace spt::lib
{

MemoryArenaBase::MemoryArenaBase(const char* arenaName, Uint64 commitedSize, Uint64 reservedSize)
	: m_arenaName(arenaName)
{
	commitedSize = math::Utils::AlignUpPow2(commitedSize, mem::GetPageSize());
	reservedSize = math::Utils::AlignUpPow2(reservedSize, mem::GetPageSize());
	SPT_CHECK(commitedSize <= reservedSize);

	m_baseAddress = (Uint64)mem::ReserveVirtualMemory(reservedSize);
	SPT_CHECK_MSG(m_baseAddress != 0u, "MemoryArena: Failed to reserve virtual memory");

	m_currentAddress = m_baseAddress;

	mem::CommitVirtualMemory((Byte*)m_baseAddress, commitedSize);

	m_commitedEnd = m_baseAddress + commitedSize;
	m_reservedEnd = m_baseAddress + reservedSize;
}

MemoryArenaBase::~MemoryArenaBase()
{
	if (m_baseAddress)
	{
		mem::ReleaseVirtualMemory((Byte*)m_baseAddress, m_reservedEnd - m_baseAddress);
	}
}

Byte* MemoryArenaBase::AllocateImpl(priv::ThreadSafeExtension& extension, Uint64 size, Uint64 alignment)
{
	SPT_CHECK(size > 0);

	const Uint64 alignMask = alignment - 1;
	SPT_CHECK((alignment & alignMask) == 0);

	std::atomic_ref<Uint64> atomicCurrentAddress{ m_currentAddress };

	Uint64 expectedAddress = atomicCurrentAddress.load(std::memory_order_relaxed);
	Uint64 allocPtr = 0u;

	while (true)
	{
		allocPtr = math::Utils::AlignUpPow2(expectedAddress, alignment);
		const Uint64 allocEnd = allocPtr + size;

		if (atomicCurrentAddress.compare_exchange_weak(expectedAddress, allocEnd))
		{
			break;
		}
	}

	if (m_currentAddress > m_commitedEnd)
	{
		lib::LockGuard lock{ extension.m_lock };

		if (m_currentAddress > m_commitedEnd) // another thread might have already committed required memory
		{
			const Uint64 newCommitedEnd = math::Utils::AlignUpPow2(m_currentAddress, mem::GetPageSize());
			SPT_CHECK(newCommitedEnd <= m_reservedEnd);

			mem::CommitVirtualMemory((Byte*)m_commitedEnd, newCommitedEnd - m_commitedEnd);
			m_commitedEnd = newCommitedEnd;
		}
	}

	return (Byte*)allocPtr;
}

Byte* MemoryArenaBase::AllocateImpl(priv::NonThreadSafeExtension& extension, Uint64 size, Uint64 alignment)
{
	SPT_CHECK(size > 0);

	const Uint64 alignMask = alignment - 1;
	SPT_CHECK((alignment & alignMask) == 0);

	m_currentAddress = math::Utils::AlignUpPow2(m_currentAddress, alignment);

	Uint64 allocPtr = m_currentAddress;
	m_currentAddress += size;

	if (m_currentAddress > m_commitedEnd)
	{
		const Uint64 newCommitedEnd = math::Utils::AlignUpPow2(m_currentAddress, mem::GetPageSize());
		SPT_CHECK(newCommitedEnd <= m_reservedEnd);

		mem::CommitVirtualMemory((Byte*)m_commitedEnd, newCommitedEnd - m_commitedEnd);
		m_commitedEnd = newCommitedEnd;
	}

	return (Byte*)allocPtr;
}

} // spt::lib
