#pragma once

#include "SculptorAliases.h"
#include "SculptorLibMacros.h"
#include "Utility/Threading/Lock.h"
#include "Containers/Span.h"
#include "Containers/ManagedSpan.h"
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

	MemoryArenaBase() = default;

	Byte* AllocateImpl(priv::ThreadSafeExtension&    extension, Uint64 size, Uint64 alignment);
	Byte* AllocateImpl(priv::NonThreadSafeExtension& extension, Uint64 size, Uint64 alignment);

	template<typename TConcreteArena>
	TConcreteArena CreateSubArenaImpl(const char* subArenaName, Byte* startPtr, Uint64 size)
	{
		TConcreteArena subArena;
		subArena.m_arenaName      = subArenaName;
		subArena.m_baseAddress    = reinterpret_cast<Uint64>(startPtr);
		subArena.m_currentAddress = subArena.m_baseAddress;
		subArena.m_commitedEnd    = subArena.m_baseAddress + size;
		subArena.m_reservedEnd    = subArena.m_baseAddress + size;

		return subArena;
	}

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

	template<typename TType>
	lib::Span<TType> AllocateSpanUninitialized(Uint64 elementsNum)
	{
		void* const ptr = (void*)Allocate(sizeof(TType) * elementsNum, alignof(TType));
		return lib::Span<TType>(reinterpret_cast<TType*>(ptr), elementsNum);
	}

	template<typename TType>
	lib::ManagedSpan<TType> AllocateArray(Uint64 elementsNum)
	{
		lib::Span<TType> span = AllocateSpanUninitialized<TType>(elementsNum);
		for (TType& element : span)
		{
			new (&element) TType();
		}
		return span;
	}

	ConcreteMemoryArena<false> CreateSubArena(const char* subArenaName, Uint64 size)
	{
		Byte* subArenaBaseAddress = Allocate(size);
		return CreateSubArenaImpl<ConcreteMemoryArena<false>>(subArenaName, subArenaBaseAddress, size);
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
