#include "RTTI.h"


namespace spt::rtti
{

struct RTTIStorage
{
	lib::MemoryArena m_arena = lib::MemoryArena("RTTIArena", 64u * 1024u, 1u * 1024u * 1024u);

	lib::HashMap<lib::RuntimeTypeInfo, const RTTIPrimitive*> m_types;
};


RTTIStorage& GetRTTIStorage()
{
	static RTTIStorage storage;
	return storage;
}


lib::MemoryArena& GetRTTIArena()
{
	return GetRTTIStorage().m_arena;
}

void RegisterType(const RTTIPrimitive& prim)
{
	GetRTTIStorage().m_types.emplace(prim.type, &prim);
}

const RTTIPrimitive* GetType(lib::RuntimeTypeInfo type)
{
	const auto it = GetRTTIStorage().m_types.find(type);
	return it != GetRTTIStorage().m_types.end() ? it->second : nullptr;
}


} // spt::rtti
