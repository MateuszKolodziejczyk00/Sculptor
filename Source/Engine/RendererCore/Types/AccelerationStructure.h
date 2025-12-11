#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIAccelerationStructureImpl.h"
#include "RendererResource.h"
#include "RendererUtils.h"
#include "DescriptorSetState/DescriptorTypes.h"


namespace spt::rdr
{

class Buffer;
class CommandRecorder;


class RENDERER_CORE_API BottomLevelAS : public RendererResource<rhi::RHIBottomLevelAS>
{
protected:

	using ResourceType = RendererResource<rhi::RHIBottomLevelAS>;

public:

	BottomLevelAS(const RendererResourceName& name, const rhi::BLASDefinition& definition);

private:

	lib::SharedPtr<Buffer> m_accelerationStructureBuffer;
	Uint64 m_accelerationStructureOffset;
};


class RENDERER_CORE_API TopLevelAS : public RendererResource<rhi::RHITopLevelAS>
{
protected:

	using ResourceType = RendererResource<rhi::RHITopLevelAS>;

public:

	TopLevelAS(const RendererResourceName& name, const rhi::TLASDefinition& definition);
	~TopLevelAS();

	const lib::SharedPtr<Buffer>& GetTLASDataBuffer() const           { return m_accelerationStructureBuffer; }
	const lib::SharedPtr<Buffer>& GetInstancesBuildDataBuffer() const { return m_instancesBuildDataBuffer; }

	void ReleaseInstancesBuildData();

	ResourceDescriptorIdx GetSRVDescriptor() const { return m_srvDescriptor; }

private:

	void InitializeSRVDescriptor();

	lib::SharedPtr<Buffer> m_accelerationStructureBuffer;
	Uint64 m_accelerationStructureOffset;

	lib::SharedPtr<Buffer> m_instancesBuildDataBuffer;

	ResourceDescriptorHandle m_srvDescriptor;
};


class RENDERER_CORE_API BLASBuilder
{
public:

	BLASBuilder();

	Bool IsEmpty() const;

	lib::SharedRef<BottomLevelAS> CreateBLAS(const RendererResourceName& name, const rhi::BLASDefinition& definition);
	
	void AddBLASToBuild(const lib::SharedRef<BottomLevelAS>& blas);

	void Build(CommandRecorder& recorder) const;

private:

	lib::DynamicArray<lib::SharedRef<BottomLevelAS>> m_blases;
};

} // spt::rdr
