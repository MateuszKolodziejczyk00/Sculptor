#include "AccelerationStructure.h"
#include "ResourcesManager.h"
#include "Buffer.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "GPUApi.h"
#include "DescriptorSetState/DescriptorManager.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// BottomLevelAS =================================================================================

BottomLevelAS::BottomLevelAS(const RendererResourceName& name, const rhi::BLASDefinition& definition)
{
	rhi::RHIBuffer accelrationStructureBuffer;

	GetRHI().InitializeRHI(definition, OUT accelrationStructureBuffer, OUT m_accelerationStructureOffset);
	GetRHI().SetName(name.Get());

	SPT_CHECK(accelrationStructureBuffer.IsValid());

	m_accelerationStructureBuffer = ResourcesManager::CreateBuffer(name, accelrationStructureBuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// TopLevelAS ====================================================================================

TopLevelAS::TopLevelAS(const RendererResourceName& name, const rhi::TLASDefinition& definition)
	: m_accelerationStructureOffset(0)
{
	rhi::RHIBuffer accelrationStructureBuffer;
	rhi::RHIBuffer instancesBuildDataBuffer;

	GetRHI().InitializeRHI(definition, OUT accelrationStructureBuffer, OUT m_accelerationStructureOffset, OUT instancesBuildDataBuffer);
	GetRHI().SetName(name.Get());

	SPT_CHECK(accelrationStructureBuffer.IsValid());
	SPT_CHECK(instancesBuildDataBuffer.IsValid());

	m_accelerationStructureBuffer = ResourcesManager::CreateBuffer(name, accelrationStructureBuffer);
	m_instancesBuildDataBuffer    = ResourcesManager::CreateBuffer(name, instancesBuildDataBuffer);

	InitializeSRVDescriptor();
}

TopLevelAS::~TopLevelAS()
{
	if (m_srvDescriptor.IsValid())
	{
		GPUApi::GetDescriptorManager().ClearDescriptorInfo(m_srvDescriptor.Get());
	}

	GPUApi::ReleaseDeferred(GPUReleaseQueue::ReleaseEntry::CreateLambda(
	[releaseTicket = GetRHI().DeferredReleaseRHI(), srvDescriptor = std::move(m_srvDescriptor)]() mutable
	{
		if (srvDescriptor.IsValid())
		{
			GPUApi::GetDescriptorManager().FreeResourceDescriptor(std::move(srvDescriptor));
		}

		releaseTicket.ExecuteReleaseRHI();
	}));
}

void TopLevelAS::ReleaseInstancesBuildData()
{
	m_instancesBuildDataBuffer.reset();
}

void TopLevelAS::InitializeSRVDescriptor()
{
	rdr::DescriptorManager& descriptorManager = rdr::GPUApi::GetDescriptorManager();

	m_srvDescriptor = descriptorManager.AllocateResourceDescriptor();
	SPT_CHECK(m_srvDescriptor.IsValid());

	descriptorManager.UploadSRVDescriptor(m_srvDescriptor, *this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// BLASBuilder ===================================================================================

BLASBuilder::BLASBuilder()
{ }

Bool BLASBuilder::IsEmpty() const
{
	return m_blases.empty();
}

lib::SharedRef<BottomLevelAS> BLASBuilder::CreateBLAS(const RendererResourceName& name, const rhi::BLASDefinition& definition)
{
	const lib::SharedRef<BottomLevelAS> blas = ResourcesManager::CreateBLAS(name, definition);

	m_blases.emplace_back(blas);

	return blas;
}

void BLASBuilder::AddBLASToBuild(const lib::SharedRef<BottomLevelAS>& blas)
{
	m_blases.emplace_back(blas);
}

void BLASBuilder::Build(CommandRecorder& recorder) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsEmpty());

	const Uint64 scratchBufferSize = std::accumulate(std::cbegin(m_blases), std::cend(m_blases), Uint64(0),
													 [](Uint64 value, const lib::SharedRef<BottomLevelAS>& blas)
													 {
														 const Uint64 requiredScratchSize = blas->GetRHI().GetBuildScratchSize();
														 return std::max<Uint64>(value, requiredScratchSize);
													 });

	const rhi::BufferDefinition scratchBufferDefinition(scratchBufferSize, lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::Storage));
	const lib::SharedRef<Buffer> scratchBuffer = ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("BLASBuilderScratchBuffer"), scratchBufferDefinition, rhi::EMemoryUsage::GPUOnly);

	for (const lib::SharedRef<BottomLevelAS>& blas : m_blases)
	{
		recorder.BuildBLAS(blas, scratchBuffer, 0);

		rhi::RHIDependency dependency;
		const SizeType bufferDependencyIdx = dependency.AddBufferDependency(scratchBuffer->GetRHI(), 0, scratchBuffer->GetRHI().GetSize());
		dependency.SetBufferDependencyStages(bufferDependencyIdx, rhi::EPipelineStage::ASBuild, rhi::EAccessType::Write, rhi::EPipelineStage::ASBuild, rhi::EAccessType::Write);

		recorder.ExecuteBarrier(dependency);
	}
}

} // spt::rdr
