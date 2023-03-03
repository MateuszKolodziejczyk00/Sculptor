#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIAccelerationStructureImpl.h"
#include "RendererResource.h"
#include "RendererUtils.h"


namespace spt::rdr
{

class Buffer;


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

	const lib::SharedPtr<Buffer>& GetInstancesBuildDataBuffer() const;

	void ReleaseInstancesBuildData();

private:

	lib::SharedPtr<Buffer> m_accelerationStructureBuffer;
	Uint64 m_accelerationStructureOffset;

	lib::SharedPtr<Buffer> m_instancesBuildDataBuffer;
};

} // spt::rdr
