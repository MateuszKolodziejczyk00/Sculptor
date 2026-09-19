#include "DescriptorManager.h"
#include "Types/DescriptorHeap.h"
#include "Vulkan/VulkanRHI.h"
#include "ResourcesManager.h"
#include "DescriptorTypes.h"
#include "Types/Texture.h"
#include "Types/Buffer.h"
#include "Types/AccelerationStructure.h"


namespace spt::rdr
{

SPT_DEFINE_LOG_CATEGORY(DescriptorManager, true);

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorsAllocator ==========================================================================

DescriptorAllocator::DescriptorAllocator(const DescriptorHeap& descriptorHeap)
	: m_descriptorsNum(descriptorHeap.GetRHI().GetDescriptorsNum())
	, m_freeDescriptorsNum(descriptorHeap.GetRHI().GetDescriptorsNum())
	, m_freeStack(descriptorHeap.GetRHI().GetDescriptorsNum(), idxNone<Uint32>)
{
	for (Uint32 idx = 0u; idx < m_descriptorsNum; ++idx)
	{
		m_freeStack[idx] = idx; // next free descriptor index
	}

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	m_descriptorsOccupation.resize(m_descriptorsNum, false);
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG
}

Uint32 DescriptorAllocator::AllocateDescriptor()
{
	Uint32 descriptorIdx = idxNone<Uint32>;

	{
		const lib::LockGuard lockGuard(m_lock);

		SPT_CHECK(m_freeDescriptorsNum > 0u);

		descriptorIdx = m_freeStack[--m_freeDescriptorsNum];

#if SPT_DESCRIPTOR_MANAGER_DEBUG
		SPT_CHECK(m_descriptorsOccupation[descriptorIdx] == false);
		m_descriptorsOccupation[descriptorIdx] = true;
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG
	}

	SPT_CHECK(descriptorIdx < m_descriptorsNum);

	return descriptorIdx;
}

void DescriptorAllocator::FreeDescriptor(Uint32 idx)
{
	SPT_CHECK(idx != idxNone<Uint32>);

	const lib::LockGuard lockGuard(m_lock);

	m_freeStack[m_freeDescriptorsNum++] = idx;

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	SPT_CHECK(m_descriptorsOccupation[idx] == true);
	m_descriptorsOccupation[idx] = false;
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG
}

#if SPT_DESCRIPTOR_MANAGER_DEBUG
Bool DescriptorAllocator::IsDescriptorOccupied(Uint32 idx) const
{
	return m_descriptorsOccupation[idx];
}
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG


//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorManager =============================================================================

DescriptorManager::DescriptorManager(DescriptorHeap& resourceDescriptorHeap, DescriptorHeap& samplerDescriptorHeap)
	: m_resourceDescriptorHeap(resourceDescriptorHeap)
	, m_samplerDescriptorHeap(samplerDescriptorHeap)
	, m_resourceDescriptorAllocator(resourceDescriptorHeap)
{
	m_resourceDescriptorInfos.resize(m_resourceDescriptorAllocator.GetDescriptorsNum());
}

DescriptorManager::~DescriptorManager()
{
	SPT_PROFILER_FUNCTION();

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	if (!m_resourceDescriptorAllocator.IsFull())
	{
		for (Uint32 idx = 0u; idx < m_resourceDescriptorAllocator.GetDescriptorsNum(); ++idx)
		{
			if (m_resourceDescriptorAllocator.IsDescriptorOccupied(idx))
			{
				const lib::String resourceName = m_resourceDescriptorInfos[idx].resourceName.ToString();

				SPT_LOG_ERROR(DescriptorManager, "Descriptor at index {} is not freed! Resource name: '{}'", idx, resourceName);
			}
		}

		SPT_CHECK_NO_ENTRY_MSG("Not all descriptors were freed!");
	}
#else
	SPT_CHECK(m_resourceDescriptorAllocator.IsFull());
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG
}

ResourceDescriptorHandle DescriptorManager::AllocateResourceDescriptor()
{
	const Uint32 descriptorIdx = m_resourceDescriptorAllocator.AllocateDescriptor();

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetDescriptorData(descriptorIdx);

	std::memset(descriptorData.data(), 0, descriptorData.size());

	return ResourceDescriptorHandle(descriptorIdx);
}

void DescriptorManager::FreeResourceDescriptor(ResourceDescriptorHandle&& handle)
{
	SPT_CHECK(handle.IsValid());

	const ResourceDescriptorIdx descriptorIndex = handle.Get();

	m_resourceDescriptorAllocator.FreeDescriptor(descriptorIndex);

	handle.Reset();

	SPT_CHECK(!handle.IsValid());
}

void DescriptorManager::UploadSRVDescriptor(ResourceDescriptorIdx idx, TextureView& textureView)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	SPT_CHECK(m_resourceDescriptorAllocator.IsDescriptorOccupied(idx));
	m_resourceDescriptorInfos[idx].resourceName = textureView.GetRHI().GetName();
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetDescriptorData(idx);

	textureView.GetRHI().CopySRVDescriptor(descriptorData);

	m_resourceDescriptorInfos[idx].Encode(&textureView);
}

void DescriptorManager::UploadUAVDescriptor(ResourceDescriptorIdx idx, TextureView& textureView)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

	SPT_CHECK(textureView.GetTexture()->GetRHI().HasUsage(rhi::ETextureUsage::StorageTexture));

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	SPT_CHECK(m_resourceDescriptorAllocator.IsDescriptorOccupied(idx));
	m_resourceDescriptorInfos[idx].resourceName = textureView.GetRHI().GetName();
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetDescriptorData(idx);

	textureView.GetRHI().CopyUAVDescriptor(descriptorData);

	m_resourceDescriptorInfos[idx].Encode(&textureView);
}

void DescriptorManager::UploadSRVDescriptor(ResourceDescriptorIdx idx, BindableBufferView& bufferView)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

	const lib::SharedRef<Buffer>& buffer = bufferView.GetBuffer();

	SPT_CHECK(lib::HasAnyFlag(buffer->GetRHI().GetUsage(), rhi::EBufferUsage::Storage));

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	SPT_CHECK(m_resourceDescriptorAllocator.IsDescriptorOccupied(idx));
	m_resourceDescriptorInfos[idx].resourceName = bufferView.GetBuffer()->GetRHI().GetName();
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetDescriptorData(idx);

	buffer->GetRHI().CopySRVDescriptor(bufferView.GetOffset(), bufferView.GetSize(), descriptorData);

	m_resourceDescriptorInfos[idx].Encode(&bufferView);
}

void DescriptorManager::UploadUAVDescriptor(ResourceDescriptorIdx idx, BindableBufferView& bufferView)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

	const lib::SharedRef<Buffer>& buffer = bufferView.GetBuffer();

	SPT_CHECK(lib::HasAnyFlag(buffer->GetRHI().GetUsage(), rhi::EBufferUsage::Storage));

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	SPT_CHECK(m_resourceDescriptorAllocator.IsDescriptorOccupied(idx));
	m_resourceDescriptorInfos[idx].resourceName = bufferView.GetBuffer()->GetRHI().GetName();
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetDescriptorData(idx);

	buffer->GetRHI().CopyUAVDescriptor(bufferView.GetOffset(), bufferView.GetSize(), descriptorData);

	m_resourceDescriptorInfos[idx].Encode(&bufferView);
}

void DescriptorManager::UploadSRVDescriptor(ResourceDescriptorIdx idx, TopLevelAS& tlas)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	SPT_CHECK(m_resourceDescriptorAllocator.IsDescriptorOccupied(idx));
	m_resourceDescriptorInfos[idx].resourceName = tlas.GetRHI().GetName();
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetDescriptorData(idx);

	tlas.GetRHI().CopySRVDescriptor(descriptorData);

	m_resourceDescriptorInfos[idx].Encode(&tlas);
}

void DescriptorManager::SetCustomDescriptorInfo(ResourceDescriptorIdx idx, void* customDataPtr)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

	m_resourceDescriptorInfos[idx].EncodeCustomPtr(customDataPtr);
}

void DescriptorManager::ClearDescriptorInfo(ResourceDescriptorIdx idx)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

	m_resourceDescriptorInfos[idx].Clear();
}

void DescriptorManager::UploadSamplerDescriptor(Uint32 idx, const rhi::SamplerDefinition& sampler)
{
	SPT_CHECK(idx < m_samplerDescriptorHeap.GetRHI().GetDescriptorsNum());

	rhi::RHIDescriptorHeap::CopySamplerDescriptor(sampler, m_samplerDescriptorHeap.GetRHI().GetDescriptorData(idx));
}

TextureView* DescriptorManager::GetTextureView(ResourceDescriptorIdx idx) const
{
	if (idx == rdr::invalidResourceDescriptorIdx)
	{
		return nullptr;
	}

	SPT_CHECK(idx < m_resourceDescriptorInfos.size());

	const DescriptorInfo& info = m_resourceDescriptorInfos[idx];
	return info.GetTextureView();
}

BindableBufferView* DescriptorManager::GetBufferView(ResourceDescriptorIdx idx) const
{
	if (idx == rdr::invalidResourceDescriptorIdx)
	{
		return nullptr;
	}

	SPT_CHECK(idx < m_resourceDescriptorInfos.size());

	const DescriptorInfo& info = m_resourceDescriptorInfos[idx];
	return info.GetBufferView();
}

void* DescriptorManager::GetCustomDescriptorInfo(ResourceDescriptorIdx idx) const
{
	if (idx == rdr::invalidResourceDescriptorIdx)
	{
		return nullptr;
	}

	SPT_CHECK(idx < m_resourceDescriptorInfos.size());

	const DescriptorInfo& info = m_resourceDescriptorInfos[idx];
	return info.GetCustomPtr();
}

debug::DescrptorBufferState DescriptorManager::DumpCurrentDescriptorBufferState() const
{
	debug::DescrptorBufferState state;
	state.slots.resize(m_resourceDescriptorInfos.size());

	for (Uint32 idx = 0u; idx < m_resourceDescriptorInfos.size(); ++idx)
	{
		const DescriptorInfo& info = m_resourceDescriptorInfos[idx];
		debug::DescriptorBufferSlotInfo& slotInfo = state.slots[idx];

		if (BindableBufferView* bufferView = info.GetBufferView())
		{
			slotInfo.bufferView = bufferView->AsSharedPtr();
		}
		else if (TextureView* textureView = info.GetTextureView())
		{
			slotInfo.textureView = textureView->shared_from_this();
		}
	}

	return state;
}

} // spt::rdr
