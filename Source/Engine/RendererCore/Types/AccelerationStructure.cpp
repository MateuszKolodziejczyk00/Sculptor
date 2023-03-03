#include "AccelerationStructure.h"
#include "ResourcesManager.h"

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

	m_accelerationStructureBuffer	= ResourcesManager::CreateBuffer(name, accelrationStructureBuffer);
	m_instancesBuildDataBuffer		= ResourcesManager::CreateBuffer(name, instancesBuildDataBuffer);
}

const lib::SharedPtr<Buffer>& TopLevelAS::GetInstancesBuildDataBuffer() const
{
	return m_instancesBuildDataBuffer;
}

void TopLevelAS::ReleaseInstancesBuildData()
{
	m_instancesBuildDataBuffer.reset();
}

} // spt::rdr
