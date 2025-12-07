#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/RHIDescriptorTypes.h"
#include "DescriptorTypes.h"
#include "DescriptorSetStateTypes.h"


namespace spt::rdr
{

#define SPT_DESCRIPTOR_MANAGER_DEBUG 0


class DescriptorHeap;
class DescriptorSetLayout;
class BindableBufferView;
class TextureView;


class DescriptorAllocator
{
public:

	DescriptorAllocator(const rhi::RHIDescriptorRange& range, const DescriptorSetLayout& layout, Uint32 binding);

	Uint32 AllocateDescriptor();
	void   FreeDescriptor(Uint32 idx);

	lib::Span<Byte> GetDescriptorData(Uint32 idx) const
	{
		SPT_CHECK(idx != idxNone<Uint32>);
		return lib::Span<Byte>{ m_descriptorsIndexer[idx], m_descriptorsIndexer.GetDescriptorSize() };
	}

	Bool IsFull() const
	{
		return m_freeDescriptorsNum == m_descriptorsIndexer.GetSize();
	}

	Uint32 GetDescriptorsNum() const
	{
		return m_descriptorsIndexer.GetSize();
	}

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	Bool IsDescriptorOccupied(Uint32 idx) const;
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG

private:

	DescriptorArrayIndexer m_descriptorsIndexer;

	lib::Spinlock m_lock;
	Uint32 m_freeDescriptorsNum = 0u;

	lib::DynamicArray<Uint32> m_freeStack;

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	lib::DynamicArray<Bool> m_descriptorsOccupation;
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG
};


class DescriptorInfo
{
	enum class EDescriptorInfoType : Uint8
	{
		TextureView = 0,
		Buffer,
		CustomPtr
	};

	EDescriptorInfoType GetType() const
	{
		return static_cast<EDescriptorInfoType>(m_ptrValue & 0x3u);
	}

	union
	{
		Uint64              m_ptrValue;
		void*               m_ptr;
		TextureView*        m_textureView;
		BindableBufferView* m_buffer;
	};

public:

#if SPT_DESCRIPTOR_MANAGER_DEBUG
	lib::HashedString resourceName;
#endif // SPT_DESCRIPTOR_MANAGER_DEBUG

	DescriptorInfo()
		: m_ptr{}
	{ }

	void Encode(TextureView* inTexture)
	{
		m_textureView = inTexture;
		m_ptrValue += static_cast<Uint64>(EDescriptorInfoType::TextureView);
	}

	void Encode(BindableBufferView* inBuffer)
	{
		m_buffer = inBuffer;
		m_ptrValue += static_cast<Uint64>(EDescriptorInfoType::Buffer);
	}

	void EncodeCustomPtr(void* inPtr)
	{
		SPT_CHECK((reinterpret_cast<Uint64>(inPtr) & 0x3ll) == 0u);
		m_ptr = inPtr;
		m_ptrValue += static_cast<Uint64>(EDescriptorInfoType::CustomPtr);
	}

	void Clear()
	{
		m_ptrValue = 0u;
	}

	Bool Contains(EDescriptorInfoType type) const
	{
		return m_ptrValue != 0u && GetType() == type;
	}

	TextureView* GetTextureView() const
	{
		return Contains(EDescriptorInfoType::TextureView) ? reinterpret_cast<TextureView*>(m_ptrValue & ~0x3ull) : nullptr;
	}

	BindableBufferView* GetBufferView() const
	{
		return Contains(EDescriptorInfoType::Buffer) ? reinterpret_cast<BindableBufferView*>(m_ptrValue & ~0x3ull) : nullptr;
	}

	void* GetCustomPtr() const
	{
		return Contains(EDescriptorInfoType::CustomPtr) ? reinterpret_cast<void*>(m_ptrValue & ~0x3ull) : nullptr;
	}
};


namespace debug
{

struct DescriptorBufferSlotInfo
{
	lib::SharedPtr<BindableBufferView> bufferView;
	lib::SharedPtr<TextureView>        textureView;
};


struct DescrptorBufferState
{
	lib::DynamicArray<DescriptorBufferSlotInfo> slots;
};

} // debug


class RENDERER_CORE_API DescriptorManager
{
public:

	DescriptorManager(DescriptorHeap& descriptorHeap);
	~DescriptorManager();

	ResourceDescriptorHandle AllocateResourceDescriptor();
	void                     FreeResourceDescriptor(ResourceDescriptorHandle&& handle);

	void UploadSRVDescriptor(ResourceDescriptorIdx idx, TextureView& textureView);
	void UploadUAVDescriptor(ResourceDescriptorIdx idx, TextureView& textureView);

	void UploadSRVDescriptor(ResourceDescriptorIdx idx, BindableBufferView& bufferView);
	void UploadUAVDescriptor(ResourceDescriptorIdx idx, BindableBufferView& bufferView);

	void SetCustomDescriptorInfo(ResourceDescriptorIdx idx, void* customDataPtr);

	void ClearDescriptorInfo(ResourceDescriptorIdx idx);

	TextureView*        GetTextureView(ResourceDescriptorIdx idx) const;
	BindableBufferView* GetBufferView(ResourceDescriptorIdx idx) const;
	void*               GetCustomDescriptorInfo(ResourceDescriptorIdx idx) const;

	Uint32 GetHeapOffset() const { return m_descriptorRange.heapOffset; }

	const lib::SharedPtr<DescriptorSetLayout>& GetBindlessLayout() const { return m_bindlessLayout; }

	debug::DescrptorBufferState DumpCurrentDescriptorBufferState() const;

private:

	lib::SharedPtr<DescriptorSetLayout> CreateBindlessLayout() const;

	DescriptorHeap& m_descriptorHeap;

	lib::SharedPtr<DescriptorSetLayout> m_bindlessLayout;

	rhi::RHIDescriptorRange m_descriptorRange;

	DescriptorAllocator m_resourceDescriptorAllocator;
	lib::DynamicArray<DescriptorInfo> m_resourceDescriptorInfos;
};

} // spt::rdr