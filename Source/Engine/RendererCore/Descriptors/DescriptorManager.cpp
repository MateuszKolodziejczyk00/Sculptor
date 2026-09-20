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

void DescriptorAllocator::Initialize(Uint32 descriptorsNum)
{
	SPT_CHECK(m_freeStack.empty());

	m_descriptorsNum     = descriptorsNum;
	m_freeDescriptorsNum = descriptorsNum;
	m_freeStack.resize(descriptorsNum, idxNone<Uint32>);

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
{
	const rhi::DescriptorProps& descriptorProps = rhi::RHI::GetDescriptorProps();

	const Uint32 heapHalfSize = static_cast<Uint32>(m_resourceDescriptorHeap.GetRHI().GetHeapSize()) / 2u;
	const Uint32 bufferDescriptorsNum  = heapHalfSize / descriptorProps.bufferDescriptorSize;
	const Uint32 textureDescriptorsNum = heapHalfSize / descriptorProps.textureDescriptorSize;

	SPT_CHECK(heapHalfSize % descriptorProps.bufferDescriptorSize == 0u);
	SPT_CHECK(heapHalfSize % descriptorProps.textureDescriptorSize == 0u);

	m_bufferDescriptorAllocator.Initialize(bufferDescriptorsNum);
	m_textureDescriptorAllocator.Initialize(textureDescriptorsNum);

	m_resourceDescriptorInfos.resize(bufferDescriptorsNum + textureDescriptorsNum);

	// In Vulkan, descriptor indexing is different from DX12, because it's based on descriptor size
	// This means that if buffer descriptor is 2x smaller than texture descriptor, idx of descriptor will kind of grow 2x faster
	// We use single array for both, and we need to make sure that indices don't overlap
	// because of that, we place all larger descriptors first and then smaller ones
	// For example, if buffer desc size is 16 bytes and texture desc size is 32 bytes, let's say that we have 256 bytes heap
	// in this case first we will have 8 texture descriptors that will use indices 0-7
	// and after that 16 buffer indices with indices 16-31
	if (descriptorProps.textureDescriptorSize > descriptorProps.bufferDescriptorSize)
	{
		m_bufferDescriptorOffset  = heapHalfSize / descriptorProps.bufferDescriptorSize;
		m_textureDescriptorOffset = 0u;

		m_bufferDescriptorInfos = lib::Span<DescriptorInfo>(m_resourceDescriptorInfos).subspan(textureDescriptorsNum, bufferDescriptorsNum);
		m_textureDescriptorInfos = lib::Span<DescriptorInfo>(m_resourceDescriptorInfos).subspan(0u, textureDescriptorsNum);
	}
	else
	{
		m_bufferDescriptorOffset  = 0u;
		m_textureDescriptorOffset = heapHalfSize / descriptorProps.textureDescriptorSize;

		m_bufferDescriptorInfos = lib::Span<DescriptorInfo>(m_resourceDescriptorInfos).subspan(0u, bufferDescriptorsNum);
		m_textureDescriptorInfos = lib::Span<DescriptorInfo>(m_resourceDescriptorInfos).subspan(bufferDescriptorsNum, textureDescriptorsNum);
	}
}

DescriptorManager::~DescriptorManager()
{
	SPT_PROFILER_FUNCTION();

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	if (!m_bufferDescriptorAllocator.IsFull())
	{
		for (Uint32 idx = 0u; idx < m_bufferDescriptorAllocator.GetDescriptorsNum(); ++idx)
		{
			if (m_bufferDescriptorAllocator.IsDescriptorOccupied(idx))
			{
				const lib::String bufferName = m_resourceDescriptorInfos[idx].resourceName.ToString();

				SPT_LOG_ERROR(DescriptorManager, "Descriptor at index {} is not freed! Resource name: '{}'", idx + m_bufferDescriptorOffset, bufferName);
			}
		}

		SPT_CHECK_NO_ENTRY_MSG("Not all descriptors were freed!");
	}
	if (!m_textureDescriptorAllocator.IsFull())
	{
		for (Uint32 idx = 0u; idx < m_textureDescriptorAllocator.GetDescriptorsNum(); ++idx)
		{
			if (m_textureDescriptorAllocator.IsDescriptorOccupied(idx))
			{
				const lib::String textureName = m_resourceDescriptorInfos[idx].resourceName.ToString();

				SPT_LOG_ERROR(DescriptorManager, "Descriptor at index {} is not freed! Resource name: '{}'", idx + m_textureDescriptorOffset, textureName);
			}
		}

		SPT_CHECK_NO_ENTRY_MSG("Not all descriptors were freed!");
	}
#else
	SPT_CHECK(m_textureDescriptorAllocator.IsFull());
	SPT_CHECK(m_bufferDescriptorAllocator.IsFull());
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG
}

ResourceDescriptorHandle DescriptorManager::AllocateBufferDescriptor()
{
	const Uint32 descriptorIdx = m_bufferDescriptorAllocator.AllocateDescriptor() + m_bufferDescriptorOffset;

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetBufferDescriptorData(descriptorIdx);

	std::memset(descriptorData.data(), 0, descriptorData.size());

	return ResourceDescriptorHandle(descriptorIdx);
}

void DescriptorManager::FreeBufferDescriptor(ResourceDescriptorHandle&& handle)
{
	SPT_CHECK(handle.IsValid());

	SPT_CHECK(handle.Get() >= m_bufferDescriptorOffset);

	const ResourceDescriptorIdx descriptorIndex = ResourceDescriptorIdx(handle.Get() - m_bufferDescriptorOffset);

	m_bufferDescriptorAllocator.FreeDescriptor(descriptorIndex);

	handle.Reset();

	SPT_CHECK(!handle.IsValid());
}

ResourceDescriptorHandle DescriptorManager::AllocateTextureDescriptor()
{
	const Uint32 descriptorIdx = m_textureDescriptorAllocator.AllocateDescriptor() + m_textureDescriptorOffset;

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetTextureDescriptorData(descriptorIdx);

	std::memset(descriptorData.data(), 0, descriptorData.size());

	return ResourceDescriptorHandle(descriptorIdx);
}

void DescriptorManager::FreeTextureDescriptor(ResourceDescriptorHandle&& handle)
{
	SPT_CHECK(handle.IsValid());

	SPT_CHECK(handle.Get() >= m_textureDescriptorOffset);

	const ResourceDescriptorIdx descriptorIndex = ResourceDescriptorIdx(handle.Get() - m_textureDescriptorOffset);

	m_textureDescriptorAllocator.FreeDescriptor(descriptorIndex);

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

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetTextureDescriptorData(idx);

	textureView.GetRHI().CopySRVDescriptor(descriptorData);

	GetDescriptorInfo(idx).Encode(&textureView);
}

void DescriptorManager::UploadUAVDescriptor(ResourceDescriptorIdx idx, TextureView& textureView)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

	SPT_CHECK(textureView.GetTexture()->GetRHI().HasUsage(rhi::ETextureUsage::StorageTexture));

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	SPT_CHECK(m_resourceDescriptorAllocator.IsDescriptorOccupied(idx));
	m_resourceDescriptorInfos[idx].resourceName = textureView.GetRHI().GetName();
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetTextureDescriptorData(idx);

	textureView.GetRHI().CopyUAVDescriptor(descriptorData);

	GetDescriptorInfo(idx).Encode(&textureView);
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

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetBufferDescriptorData(idx);

	buffer->GetRHI().CopySRVDescriptor(bufferView.GetOffset(), bufferView.GetSize(), descriptorData);

	GetDescriptorInfo(idx).Encode(&bufferView);
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

	const lib::Span<Byte> descriptorData = m_resourceDescriptorHeap.GetRHI().GetBufferDescriptorData(idx);

	buffer->GetRHI().CopyUAVDescriptor(bufferView.GetOffset(), bufferView.GetSize(), descriptorData);

	GetDescriptorInfo(idx).Encode(&bufferView);
}

void DescriptorManager::SetCustomDescriptorInfo(ResourceDescriptorIdx idx, void* customDataPtr)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

	GetDescriptorInfo(idx).EncodeCustomPtr(customDataPtr);
}

void DescriptorManager::ClearDescriptorInfo(ResourceDescriptorIdx idx)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

	GetDescriptorInfo(idx).Clear();
}

void DescriptorManager::UploadSamplerDescriptor(Uint32 idx, const rhi::SamplerDefinition& sampler)
{
	SPT_CHECK(idx < m_samplerDescriptorHeap.GetRHI().GetSamplerDescriptorsNum());

	rhi::RHIDescriptorHeap::CopySamplerDescriptor(sampler, m_samplerDescriptorHeap.GetRHI().GetSamplerDescriptorData(idx));
}

TextureView* DescriptorManager::GetTextureView(ResourceDescriptorIdx idx) const
{
	if (idx == rdr::invalidResourceDescriptorIdx)
	{
		return nullptr;
	}

	const DescriptorInfo& info = GetDescriptorInfo(idx);
	return info.GetTextureView();
}

BindableBufferView* DescriptorManager::GetBufferView(ResourceDescriptorIdx idx) const
{
	if (idx == rdr::invalidResourceDescriptorIdx)
	{
		return nullptr;
	}

	const DescriptorInfo& info = GetDescriptorInfo(idx);
	return info.GetBufferView();
}

void* DescriptorManager::GetCustomDescriptorInfo(ResourceDescriptorIdx idx) const
{
	if (idx == rdr::invalidResourceDescriptorIdx)
	{
		return nullptr;
	}

	const DescriptorInfo& info = GetDescriptorInfo(idx);
	return info.GetCustomPtr();
}

debug::DescrptorBufferState DescriptorManager::DumpCurrentDescriptorBufferState() const
{
	debug::DescrptorBufferState state;

	// We have to write unresolved indices here
	const rhi::DescriptorProps& descriptorProps = rhi::RHI::GetDescriptorProps();
	const Uint32 slotsNum = static_cast<Uint32>(m_resourceDescriptorHeap.GetRHI().GetHeapSize()) / std::min(descriptorProps.bufferDescriptorSize, descriptorProps.textureDescriptorSize);
	state.slots.resize(slotsNum);

	for (Uint32 idx = 0u; idx < m_bufferDescriptorInfos.size(); ++idx)
	{
		const DescriptorInfo& info = m_bufferDescriptorInfos[idx];
		debug::DescriptorBufferSlotInfo& slotInfo = state.slots[idx + m_bufferDescriptorOffset];

		if (BindableBufferView* bufferView = info.GetBufferView())
		{
			slotInfo.bufferView = bufferView->AsSharedPtr();
		}
	}

	for (Uint32 idx = 0u; idx < m_textureDescriptorInfos.size(); ++idx)
	{
		const DescriptorInfo& info = m_textureDescriptorInfos[idx];
		debug::DescriptorBufferSlotInfo& slotInfo = state.slots[idx + m_textureDescriptorOffset];

		if (TextureView* textureView = info.GetTextureView())
		{
			slotInfo.textureView = textureView->AsShared();
		}
	}

	return state;
}

DescriptorInfo& DescriptorManager::GetDescriptorInfo(ResourceDescriptorIdx idx)
{
	const Bool isBuffer = idx >= m_bufferDescriptorOffset && idx < m_bufferDescriptorOffset + m_bufferDescriptorAllocator.GetDescriptorsNum();
	if (isBuffer)
	{
		SPT_CHECK(idx >= m_bufferDescriptorOffset && idx < m_bufferDescriptorOffset + m_bufferDescriptorAllocator.GetDescriptorsNum());
		return m_bufferDescriptorInfos[idx - m_bufferDescriptorOffset];
	}
	else
	{
		SPT_CHECK(idx >= m_textureDescriptorOffset && idx < m_textureDescriptorOffset + m_textureDescriptorAllocator.GetDescriptorsNum());
		return m_textureDescriptorInfos[idx - m_textureDescriptorOffset];
	}
}

const DescriptorInfo& DescriptorManager::GetDescriptorInfo(ResourceDescriptorIdx idx) const
{
	return const_cast<DescriptorManager*>(this)->GetDescriptorInfo(idx);
}

} // spt::rdr
