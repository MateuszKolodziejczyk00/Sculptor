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

	ResourceDescriptorIdx GetSRVDescriptor() const { return m_srvDescriptor; }

private:

	void InitializeSRVDescriptor();

	lib::SharedPtr<Buffer> m_accelerationStructureBuffer;
	Uint64 m_accelerationStructureOffset;

	ResourceDescriptorHandle m_srvDescriptor;
};


class RENDERER_CORE_API BLASBuilder
{
public:

	struct BuildCommand
	{
		lib::SharedRef<BottomLevelAS> blas;
		rhi::BLASBuildInfo            buildInfo;
	};

	BLASBuilder();

	Bool IsEmpty() const;

	lib::SharedRef<BottomLevelAS> CreateBLAS(const RendererResourceName& name, const rhi::BLASDefinition& definition, const rhi::BLASBuildInfo& buildInfo);
	
	void AddBLASToBuild(BuildCommand command);

	void Build(CommandRecorder& recorder) const;

private:

	lib::DynamicArray<BuildCommand> m_commands;
};

} // spt::rdr
