#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"

namespace spt::rsc
{

class RenderInstancesListBase abstract
{
public:

	explicit RenderInstancesListBase(const rdr::RendererResourceName& listName, Uint64 dataSize, Uint64 instanceDataAlignment);

	const lib::SharedPtr<rdr::Buffer>& GetInstancesBuffer() const;

protected:

	rhi::RHISuballocation	AddInstanceImpl(const Byte* instanceData, Uint64 instanceDataSize);
	void					RemoveInstanceImpl(const rhi::RHISuballocation& instanceSuballocation);
	void					FlushRemovedInstancesImpl();

private:

	lib::SharedPtr<rdr::Buffer> m_instances;

	const Uint64 m_instanceDataAlignment;
};


template<typename TInstanceDataType>
class RenderInstancesList : public RenderInstancesListBase
{
protected:

	using Super = RenderInstancesListBase;

public:

	explicit RenderInstancesList(const rdr::RendererResourceName& listName, Uint64 maxInstancesNum)
		: Super(listName, maxInstancesNum * sizeof(TInstanceDataType), sizeof(TInstanceDataType))
	{ }

	rhi::RHISuballocation AddInstance(const TInstanceDataType& instanceData)
	{
		return AddInstanceImpl(reinterpret_cast<const Byte*>(&instanceData), sizeof(TInstanceDataType));
	}

	void RemoveInstance(const rhi::RHISuballocation& instanceSuballocation)
	{
		return RemoveInstanceImpl(instanceSuballocation);
	}

	void FlushRemovedInstances()
	{
		return FlushRemovedInstancesImpl();
	}
};

} // spt::rsc