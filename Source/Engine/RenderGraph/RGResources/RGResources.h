#pragma once

#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"
#include "RGResources/RGTrackedObject.h"
#include "RGResources/RGResourceHandles.h"
#include "Types/Texture.h"
#include "Types/Buffer.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RHICore/RHIBufferTypes.h"
#include "RHICore/RHISynchronizationTypes.h"
#include "RHICore/RHIShaderTypes.h"
#include "Utility/Templates/TypeStorage.h"
#include "RGResourceTypes.h"


namespace spt::rg
{


struct RGResourceDef
{
	RGResourceDef()
		: flags(ERGResourceFlags::None)
	{ }

	RenderGraphDebugName	name;
	ERGResourceFlags		flags;
};


class RGResource abstract : public RGTrackedObject
{
public:

	explicit RGResource(const RGResourceDef& definition)
		: m_name(definition.name)
		, m_flags(definition.flags)
	{ }

	const lib::HashedString& GetName() const
	{
		return m_name.Get();
	}

	ERGResourceFlags GetFlags() const
	{
		return m_flags;
	}

	Bool HasAcquireNode() const
	{
		return m_acquireNode.IsValid();
	}

	void SetAcquireNode(RGNodeHandle node)
	{
		m_acquireNode = node;
	}

	RGNodeHandle GetAcquireNode() const
	{
		return m_acquireNode;
	}

	Bool HasReleaseNode() const
	{
		return m_releaseNode.IsValid();
	}

	void SetReleaseNode(RGNodeHandle node)
	{
		m_releaseNode = node;
	}

	RGNodeHandle GetReleaseNode() const
	{
		return m_releaseNode;
	}

	Bool IsExternal() const
	{
		return lib::HasAnyFlag(GetFlags(), ERGResourceFlags::External);
	}

private:

	RenderGraphDebugName	m_name;
	ERGResourceFlags		m_flags;

	// Node that initializes this resource
	RGNodeHandle	m_acquireNode;
	// Node that releases this resource
	RGNodeHandle	m_releaseNode;
};


class RGTexture : public RGResource
{
public:

	RGTexture(const RGResourceDef& resourceDefinition, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
		: RGResource(resourceDefinition)
		, m_textureDefinition(textureDefinition)
		, m_allocationInfo(allocationInfo)
		, m_isAcquired(false)
		, m_accessState(textureDefinition.mipLevels, textureDefinition.arrayLayers)
		, m_extractionDest(nullptr)
		, m_releaseTransitionTarget(nullptr)
	{ }

	RGTexture(const RGResourceDef& resourceDefinition, lib::SharedPtr<rdr::Texture> texture)
		: RGResource(resourceDefinition)
		, m_textureDefinition(texture->GetRHI().GetDefinition())
		, m_allocationInfo(texture->GetRHI().GetAllocationInfo())
		, m_isAcquired(true)
		, m_texture(texture)
		, m_accessState(texture->GetRHI().GetDefinition().mipLevels, texture->GetRHI().GetDefinition().arrayLayers)
		, m_extractionDest(nullptr)
		, m_releaseTransitionTarget(nullptr)
	{
		SPT_CHECK(IsExternal());
	}
	
	// Texture Definition ==================================================
	
	const rhi::TextureDefinition& GetTextureDefinition() const
	{
		return m_textureDefinition;
	}

	const rhi::RHIAllocationInfo& GetAllocationInfo() const
	{
		return m_allocationInfo;
	}

	rhi::ETextureUsage GetUsage() const
	{
		return GetTextureDefinition().usage;
	}

	const math::Vector3u& GetResolution() const
	{
		return GetTextureDefinition().resolution;
	}

	// Texture Resource ====================================================

	Bool IsAcquired() const
	{
		return m_isAcquired;
	}
	
	const lib::SharedPtr<rdr::Texture>& GetResource(Bool onlyIfAcquired = true) const
	{
		SPT_CHECK(!onlyIfAcquired || IsAcquired());
		return m_texture;
	}

	void AcquireResource(lib::SharedPtr<rdr::Texture> texture)
	{
		SPT_CHECK(!IsAcquired());
		m_texture = std::move(texture);
		m_isAcquired = true;
	}

	lib::SharedPtr<rdr::Texture> ReleaseResource()
	{
		m_isAcquired = false;
		return m_texture;
	}

	// Extraction ==========================================================

	void SetExtractionDestination(lib::SharedPtr<rdr::Texture>& destination)
	{
		SPT_CHECK_MSG(!IsExtracted(), "Texture cannot be extracted twice");
		m_extractionDest = &destination;
	}

	Bool IsExtracted() const
	{
		return !!m_extractionDest;
	}

	lib::SharedPtr<rdr::Texture>& GetExtractionDestChecked() const
	{
		SPT_CHECK(IsExtracted());
		return *m_extractionDest;
	}

	// Access State ========================================================

	RGTextureAccessState& GetAccessState()
	{
		return m_accessState;
	}

	// Relase ==============================================================

	void SetReleaseTransitionTarget(const rhi::BarrierTextureTransitionDefinition* transitionTarget)
	{
		SPT_CHECK_MSG(!m_releaseTransitionTarget, "Release transition already defined");
		m_releaseTransitionTarget = transitionTarget;
	}

	const rhi::BarrierTextureTransitionDefinition* GetReleaseTransitionTarget() const
	{
		return m_releaseTransitionTarget;
	}

private:

	rhi::TextureDefinition m_textureDefinition;
	rhi::RHIAllocationInfo m_allocationInfo;

	Bool m_isAcquired;

	lib::SharedPtr<rdr::Texture> m_texture;

	RGTextureAccessState m_accessState;

	lib::SharedPtr<rdr::Texture>*					m_extractionDest;

	const rhi::BarrierTextureTransitionDefinition*	m_releaseTransitionTarget;
};


class RGTextureView : public RGResource
{
public:

	RGTextureView(const RGResourceDef& resourceDefinition, RGTextureHandle texture, const rhi::TextureViewDefinition& viewDefinition)
		: RGResource(resourceDefinition)
		, m_texture(texture)
		, m_viewDef(viewDefinition)
	{ }

	RGTextureView(const RGResourceDef& resourceDefinition, RGTextureHandle texture, const lib::SharedRef<rdr::TextureView>& textureView)
		: RGResource(resourceDefinition)
		, m_texture(texture)
		, m_textureView(textureView)
	{
		SPT_CHECK(IsExternal());
	}

	RGTextureHandle GetTexture() const
	{
		return m_texture;
	}

	const rhi::TextureSubresourceRange& GetSubresourceRange() const
	{
		return IsExternal() ? m_textureView->GetRHI().GetSubresourceRange() : GetViewDefinition().subresourceRange;
	}

	const math::Vector3u& GetResolution() const
	{
		return GetTexture()->GetResolution();
	}

	const lib::SharedPtr<rdr::TextureView>& GetViewInstance() const
	{
		if (!m_textureView)
		{
			m_textureView = CreateView();
		}

		SPT_CHECK(!!m_textureView);
		return m_textureView;
	}

	// Texture Resource ====================================================

	Bool IsAcquired() const
	{
		return m_textureView && m_texture->IsAcquired();
	}
	
	const lib::SharedPtr<rdr::TextureView>& GetResource(Bool onlyIfAcquired = true) const
	{
		SPT_CHECK(!onlyIfAcquired || IsAcquired());
		return m_textureView;
	}

	void AcquireResource()
	{
		// texture views are not reused so they don't need to be released after acquire
		SPT_CHECK(!IsAcquired());
		m_textureView = CreateView();
	}

private:

	lib::SharedPtr<rdr::TextureView> CreateView() const
	{
		SPT_CHECK(m_texture.IsValid());
		SPT_CHECK(m_texture->IsAcquired());

		const lib::SharedPtr<rdr::Texture>& textureInstance = m_texture->GetResource();
		SPT_CHECK(!!textureInstance);

		return textureInstance->CreateView(RENDERER_RESOURCE_NAME(GetName()), GetViewDefinition());
	}

	const rhi::TextureViewDefinition& GetViewDefinition() const
	{
		SPT_CHECK_MSG(!IsExternal(), "View Definition is not valid for external texture views");
		return m_viewDef;
	}

	RGTextureHandle				m_texture;
	rhi::TextureViewDefinition	m_viewDef;

	mutable lib::SharedPtr<rdr::TextureView> m_textureView;
};


class RGBuffer : public RGResource
{
public:

	RGBuffer(const RGResourceDef& resourceDefinition, const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
		: RGResource(resourceDefinition)
		, m_bufferDef(definition)
		, m_allocationInfo(allocationInfo)
		, m_isAcquired(false)
		, m_extractionDest(nullptr)
	{ }

	RGBuffer(const RGResourceDef& resourceDefinition, lib::SharedPtr<rdr::Buffer> bufferInstance)
		: RGResource(resourceDefinition)
		, m_isAcquired(true)
		, m_bufferInstance(std::move(bufferInstance))
		, m_extractionDest(nullptr)
	{
		SPT_CHECK(lib::HasAnyFlag(GetFlags(), ERGResourceFlags::External));
		SPT_CHECK(IsAcquired());
	}

	const rhi::BufferDefinition& GetBufferDefinition() const
	{
		SPT_CHECK_MSG(!IsExternal(), "Definition is invalid for external buffers");
		return m_bufferDef;
	}

	const rhi::RHIAllocationInfo& GetAllocationInfo() const
	{
		SPT_CHECK_MSG(!IsExternal(), "Allocation Info is invalid for external buffers");
		return m_allocationInfo;
	}

	rhi::EBufferUsage GetUsageFlags() const
	{
		return IsExternal() ? m_bufferInstance->GetRHI().GetUsage() : GetBufferDefinition().usage;
	}

	Bool AllowsHostWrites() const
	{
		if (IsExternal())
		{
			return m_bufferInstance->GetRHI().CanMapMemory();
		}
		else
		{
			return m_allocationInfo.memoryUsage != rhi::EMemoryUsage::GPUOnly;
		}
	}

	// Resource ============================================================

	Bool IsAcquired() const
	{
		return !!m_bufferInstance;
	}
	
	void AcquireResource(lib::SharedPtr<rdr::Buffer> buffer)
	{
		SPT_CHECK(!m_bufferInstance);
		m_bufferInstance = std::move(buffer);
		m_isAcquired = true;
	}

	lib::SharedPtr<rdr::Buffer> ReleaseResource()
	{
		m_isAcquired = false;
		return m_bufferInstance;
	}

	const lib::SharedPtr<rdr::Buffer>& GetResource(Bool onlyIfAcquired = true) const
	{
		SPT_CHECK(!onlyIfAcquired || IsAcquired());
		return m_bufferInstance;
	}

	// Extraction ==========================================================

	Bool IsExtracted() const
	{
		return !!m_extractionDest;
	}

	lib::SharedPtr<rdr::Buffer>* GetExtractionDest() const
	{
		return m_extractionDest;
	}

	void SetExtractionDest(lib::SharedPtr<rdr::Buffer>* dest)
	{
		SPT_CHECK_MSG(!IsExtracted(), "This resource was already extracted");
		m_extractionDest = dest;
	}

	// Access Synchronization ==============================================

	RGNodeHandle GetLastAccessNode() const
	{
		return m_lastAccessNode;
	}

	void SetLastAccessNode(RGNodeHandle node)
	{
		m_lastAccessNode = node;
	}

private:

	rhi::BufferDefinition	m_bufferDef;
	rhi::RHIAllocationInfo	m_allocationInfo;

	Bool m_isAcquired;

	RGNodeHandle			m_lastAccessNode;

	lib::SharedPtr<rdr::Buffer> m_bufferInstance;

	lib::SharedPtr<rdr::Buffer>* m_extractionDest;
};


class RGBufferView : public RGResource
{
public:

	RGBufferView(const RGResourceDef& resourceDefinition, RGBufferHandle buffer, Uint64 offset, Uint64 size)
		: RGResource(resourceDefinition)
		, m_buffer(buffer)
		, m_offset(offset)
		, m_size(size)
		, m_lastAccess(ERGBufferAccess::Unknown)
		, m_lastAccessPipelineStages(rhi::EPipelineStage::None)
		, m_earliestStageReadAfterLastWrite(rhi::EPipelineStage::BottomOfPipe)
		, m_hasValidInstance(false)
	{
		SPT_CHECK(m_buffer.IsValid());
	}

	RGBufferView(const RGResourceDef& resourceDefinition, RGBufferHandle buffer, const rdr::BufferView& bufferView)
		: RGResource(resourceDefinition)
		, m_buffer(buffer)
		, m_offset(bufferView.GetOffset())
		, m_size(bufferView.GetSize())
		, m_lastAccess(ERGBufferAccess::Unknown)
		, m_lastAccessPipelineStages(rhi::EPipelineStage::None)
		, m_earliestStageReadAfterLastWrite(rhi::EPipelineStage::BottomOfPipe)
		, m_bufferViewInstance(bufferView)
		, m_hasValidInstance(true)
	{
		SPT_CHECK(m_buffer.IsValid());
		SPT_CHECK(IsExternal());
	}

	~RGBufferView()
	{
		if (m_hasValidInstance)
		{
			m_bufferViewInstance.Destroy();
		}
	}

	Uint64 GetOffset() const
	{
		return m_offset;
	}

	Uint64 GetSize() const
	{
		return m_size;
	}

	RGBufferHandle GetBuffer() const
	{
		return m_buffer;
	}

	Bool AllowsHostWrites() const
	{
		return m_buffer->AllowsHostWrites();
	}

	const rdr::BufferView& GetBufferViewInstance(Bool onlyIfAcquired = true) const
	{
		SPT_CHECK(!onlyIfAcquired || m_buffer->IsAcquired());
		if (!m_hasValidInstance)
		{
			const lib::SharedPtr<rdr::Buffer> owningBufferInstance = m_buffer->GetResource();
			SPT_CHECK(!!owningBufferInstance);

			m_bufferViewInstance.Construct(lib::Ref(owningBufferInstance), m_offset, m_size);
			m_hasValidInstance = true;
		}

		return m_bufferViewInstance.Get();
	}

	// Access Synchronization ==============================================

	RGNodeHandle GetLastAccessNode() const
	{
		return m_lastAccessNode;
	}

	void SetLastAccessNode(RGNodeHandle node)
	{
		m_lastAccessNode = node;
	}

	ERGBufferAccess GetLastAccessType() const
	{
		return m_lastAccess;
	}

	void SetLastAccessType(ERGBufferAccess access)
	{
		m_lastAccess = access;
	}

	rhi::EPipelineStage GetLastAccessPipelineStages() const
	{
		return m_lastAccessPipelineStages;
	}

	void SetLastAccessPipelineStages(rhi::EPipelineStage stages)
	{
		m_lastAccessPipelineStages = stages;
	}
	
	void SetEarliestStageReadAfterLastWrite(rhi::EPipelineStage stage)
	{
		m_earliestStageReadAfterLastWrite = stage;
	}

	rhi::EPipelineStage GetEarliestStageReadAfterLastWrite() const
	{
		return m_earliestStageReadAfterLastWrite;
	}

private:

	RGBufferHandle m_buffer;

	Uint64 m_offset;
	Uint64 m_size;

	RGNodeHandle			m_lastAccessNode;
	ERGBufferAccess			m_lastAccess;
	rhi::EPipelineStage		m_lastAccessPipelineStages;

	rhi::EPipelineStage		m_earliestStageReadAfterLastWrite;

	mutable lib::TypeStorage<rdr::BufferView>	m_bufferViewInstance;
	mutable Bool								m_hasValidInstance;
};

} // spt::rg


namespace std
{

template<>
struct hash<spt::rg::RGTextureAccessState>
{
	size_t operator()(const spt::rg::RGTextureSubresourceAccessState& accessState) const
	{
		return spt::lib::HashCombine(accessState.lastAccessType, accessState.lastAccessNode);
	}
};

} // std