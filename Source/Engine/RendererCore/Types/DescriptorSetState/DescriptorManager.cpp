#include "DescriptorManager.h"
#include "Types/DescriptorHeap.h"
#include "Vulkan/VulkanRHI.h"
#include "Types/DescriptorSetLayout.h"
#include "ResourcesManager.h"
#include "DescriptorSetStateTypes.h"
#include "Types/Texture.h"
#include "Types/Buffer.h"
#include "Types/Sampler.h"


namespace spt::rdr
{

SPT_DEFINE_LOG_CATEGORY(DescriptorManager, true);

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorsAllocator ==========================================================================

DescriptorAllocator::DescriptorAllocator(const rhi::RHIDescriptorRange& range, const DescriptorSetLayout& layout, Uint32 binding)
	: m_descriptorsIndexer(DescriptorSetIndexer(range.data, layout)[binding])
	, m_freeDescriptorsNum(m_descriptorsIndexer.GetSize())
	, m_freeStack(m_descriptorsIndexer.GetSize(), idxNone<Uint32>)
{
	for (Uint32 idx = 0u; idx < m_descriptorsIndexer.GetSize(); ++idx)
	{
		m_freeStack[idx] = idx; // next free descriptor index
	}

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	m_descriptorsOccupation.resize(m_descriptorsIndexer.GetSize(), false);
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

	SPT_CHECK(descriptorIdx < m_descriptorsIndexer.GetSize());

	const lib::Span<Byte> descriptorData = GetDescriptorData(descriptorIdx);

	std::memset(descriptorData.data(), 0, descriptorData.size());

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

DescriptorManager::DescriptorManager(DescriptorHeap& descriptorHeap)
	: m_descriptorHeap(descriptorHeap)
	, m_bindlessLayout(CreateBindlessLayout())
	, m_descriptorRange(descriptorHeap.GetRHI().AllocateRange(m_bindlessLayout->GetRHI().GetDescriptorsDataSize()))
	, m_resourceDescriptorAllocator(m_descriptorRange, *m_bindlessLayout, 0u)
	, m_samplerDescriptorsIndexer(DescriptorSetIndexer(m_descriptorRange.data, *m_bindlessLayout)[1])
{
	m_resourceDescriptorInfos.resize(m_resourceDescriptorAllocator.GetDescriptorsNum());
}

DescriptorManager::~DescriptorManager()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_descriptorRange.IsValid());

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

	m_descriptorHeap.GetRHI().DeallocateRange(m_descriptorRange);
}

ResourceDescriptorHandle DescriptorManager::AllocateResourceDescriptor()
{
	return ResourceDescriptorHandle(std::move(m_resourceDescriptorAllocator.AllocateDescriptor()));
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

	const lib::Span<Byte> descriptorData = m_resourceDescriptorAllocator.GetDescriptorData(idx);

	textureView.GetRHI().CopySRVDescriptor(descriptorData.data());

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

	const lib::Span<Byte> descriptorData = m_resourceDescriptorAllocator.GetDescriptorData(idx);

	textureView.GetRHI().CopyUAVDescriptor(descriptorData.data());

	m_resourceDescriptorInfos[idx].Encode(&textureView);
}

void DescriptorManager::UploadSRVDescriptor(ResourceDescriptorIdx idx, BindableBufferView& bufferView)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

	const lib::SharedRef<Buffer>& buffer = bufferView.GetBuffer();

	SPT_CHECK(lib::HasAnyFlag(buffer->GetRHI().GetUsage(), rhi::EBufferUsage::Storage));

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	SPT_CHECK(m_resourceDescriptorAllocator.IsDescriptorOccupied(idx));
	m_resourceDescriptorInfos[idx].resourceName = textureView.GetRHI().GetName();
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG

	const lib::Span<Byte> descriptorData = m_resourceDescriptorAllocator.GetDescriptorData(idx);

	buffer->GetRHI().CopySRVDescriptor(bufferView.GetOffset(), bufferView.GetSize(), descriptorData.data());

	m_resourceDescriptorInfos[idx].Encode(&bufferView);
}

void DescriptorManager::UploadUAVDescriptor(ResourceDescriptorIdx idx, BindableBufferView& bufferView)
{
	SPT_CHECK(idx != rdr::invalidResourceDescriptorIdx);

	const lib::SharedRef<Buffer>& buffer = bufferView.GetBuffer();

	SPT_CHECK(lib::HasAnyFlag(buffer->GetRHI().GetUsage(), rhi::EBufferUsage::Storage));

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	SPT_CHECK(m_resourceDescriptorAllocator.IsDescriptorOccupied(idx));
	m_resourceDescriptorInfos[idx].resourceName = textureView.GetRHI().GetName();
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG

	const lib::Span<Byte> descriptorData = m_resourceDescriptorAllocator.GetDescriptorData(idx);

	buffer->GetRHI().CopyUAVDescriptor(bufferView.GetOffset(), bufferView.GetSize(), descriptorData.data());

	m_resourceDescriptorInfos[idx].Encode(&bufferView);
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

void DescriptorManager::UploadSamplerDescriptor(Uint32 idx, Sampler& sampler)
{
	SPT_CHECK(idx < m_samplerDescriptorsIndexer.GetSize());

	const rhi::RHISampler& rhiSampler = sampler.GetRHI();

	rhiSampler.CopyDescriptor(m_samplerDescriptorsIndexer[idx]);
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

lib::SharedPtr<DescriptorSetLayout> DescriptorManager::CreateBindlessLayout() const
{
	constexpr Uint64 descriptorsNum = 1024u * 128u;

	rhi::DescriptorSetBindingDefinition resourcesBinding;
	resourcesBinding.bindingIdx      = 0u;
	resourcesBinding.descriptorType  = rhi::EDescriptorType::CBV_SRV_UAV;
	resourcesBinding.descriptorCount = descriptorsNum;
	resourcesBinding.shaderStages    = rhi::EShaderStageFlags::All;
	resourcesBinding.flags           = rhi::EDescriptorSetBindingFlags::PartiallyBound;

	rhi::DescriptorSetBindingDefinition samplersBinding;
	samplersBinding.bindingIdx      = 1u,
	samplersBinding.descriptorType  = rhi::EDescriptorType::Sampler,
	samplersBinding.descriptorCount = 16u,
	samplersBinding.shaderStages    = rhi::EShaderStageFlags::All;
	samplersBinding.flags           = rhi::EDescriptorSetBindingFlags::PartiallyBound;

	rhi::DescriptorSetDefinition bindlessSetDef;
	bindlessSetDef.bindings.emplace_back(resourcesBinding);
	bindlessSetDef.bindings.emplace_back(samplersBinding);

	return ResourcesManager::CreateDescriptorSetLayout(RENDERER_RESOURCE_NAME("Bindless Layout"), bindlessSetDef);
}

} // spt::rdr
