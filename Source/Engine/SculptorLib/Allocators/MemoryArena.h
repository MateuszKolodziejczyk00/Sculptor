#pragma once

#include "SculptorAliases.h"
#include "SculptorLibMacros.h"
#include "Utility/Threading/Lock.h"
#include <utility>


namespace spt::lib
{

namespace priv
{

struct ThreadSafeExtension
{
	lib::Lock m_lock;
};

struct NonThreadSafeExtension {};

} // priv


class SCULPTOR_LIB_API MemoryArenaBase
{
public:

	MemoryArenaBase(const char* arenaName, Uint64 commitedSize, Uint64 reservedSize);
	~MemoryArenaBase();

	void Reset() { m_currentAddress = m_baseAddress; }

protected:

	Byte* AllocateImpl(priv::ThreadSafeExtension&    extension, Uint64 size, Uint64 alignment);
	Byte* AllocateImpl(priv::NonThreadSafeExtension& extension, Uint64 size, Uint64 alignment);

private:

	friend class MemoryArenaScope;

	const char* m_arenaName = nullptr;
	Uint64 m_baseAddress     = 0u;
	Uint64 m_currentAddress  = 0u;
	Uint64 m_commitedEnd     = 0u;
	Uint64 m_reservedEnd     = 0u;
};


template<Bool isThreadSafe>
class SCULPTOR_LIB_API ConcreteMemoryArena : public MemoryArenaBase
{
public:

	using MemoryArenaBase::MemoryArenaBase;

	Byte* Allocate(Uint64 size, Uint64 alignment = alignof(std::max_align_t));

	template<typename TType, typename... TArgs>
	TType* AllocateType(TArgs&&... args)
	{
		void* const ptr = (void*)Allocate(sizeof(TType), alignof(TType));
		return new (ptr) TType(std::forward<TArgs>(args)...);
	}

private:

	using TExtension = std::conditional_t<isThreadSafe, priv::ThreadSafeExtension, priv::NonThreadSafeExtension>;
	TExtension m_extension;
};


template<Bool isThreadSafe>
Byte* ConcreteMemoryArena<isThreadSafe>::Allocate(Uint64 size, Uint64 alignment)
{
	return AllocateImpl(m_extension, size, alignment);
}


using MemoryArena           = ConcreteMemoryArena<false>;
using ThreadSafeMemoryArena = ConcreteMemoryArena<true>;


class MemoryArenaScope
{
public:

	MemoryArenaScope(MemoryArena& arena)
		: m_arena(arena)
		, m_savedAddress(arena.m_currentAddress)
	{ }

	~MemoryArenaScope()
	{
		m_arena.m_currentAddress = m_savedAddress;
	}

private:

	MemoryArena& m_arena;
	Uint64       m_savedAddress;
};

} // spt::lib;
