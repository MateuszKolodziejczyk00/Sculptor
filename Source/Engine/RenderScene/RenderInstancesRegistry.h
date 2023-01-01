#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"

namespace spt::rsc
{

class RenderInstnacesList
{
public:

	explicit RenderInstnacesList(const rdr::RendererResourceName& listName, Uint32 maxInstancesNum);

	Uint32 AddInstance(Uint32 instanceDataOffset);
	void RemoveInstance(Uint32 idx);

	const lib::SharedPtr<rdr::Buffer>& GetInstancesBuffer() const;

	const lib::SharedPtr<rdr::Buffer>& GetInstancesToRemoveBuffer() const;
	const lib::SharedPtr<rdr::Buffer>& GetInstancesToRemoveCountBuffer() const;

private:

	lib::SharedPtr<rdr::Buffer> m_instances;

	lib::SharedPtr<rdr::Buffer> m_instancesToRemove;
	lib::SharedPtr<rdr::Buffer> m_instancesToRemoveCount;

	Uint32 m_lastAppendIdx;

	// Probably may be changed to atomics, but then every idx in m_instances buffer would have to be atomic_ref
	lib::Lock m_addLock;
};


class RenderInstancesRegistry
{
public:

	RenderInstancesRegistry();

	RenderInstnacesList& GetBasePassInstances();

private:

	RenderInstnacesList m_basePassInstances;
};

} // spt::rsc