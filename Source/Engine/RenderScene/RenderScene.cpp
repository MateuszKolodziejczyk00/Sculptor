#include "RenderScene.h"
#include "ResourcesManager.h"
#include "Utils/TransfersUtils.h"
#include "EngineFrame.h"


namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderScene ===================================================================================

RenderScene::RenderScene()
	: m_instances("RenderSceneInstancesPool")
	, m_renderEntitiesBuffer(CreateInstancesBuffer())
{
}

void RenderScene::BeginFrame(const engn::FrameContext& frame)
{
	SPT_CHECK(!m_currentFrame);
	m_currentFrame = frame.shared_from_this();
}

void RenderScene::EndFrame()
{
	SPT_CHECK(!!m_currentFrame);
	m_currentFrame.reset();

	lighting.EndFrame();
}

const engn::FrameContext& RenderScene::GetCurrentFrameRef() const
{
	SPT_CHECK_MSG(m_currentFrame, "No current frame!");
	return *m_currentFrame;
}

RenderInstanceHandle RenderScene::CreateInstance(const RenderInstanceDef& def)
{
	SPT_PROFILER_FUNCTION();

	RenderInstance instanceData;
	instanceData.transform = def.transform;
	const RenderInstanceHandle instanceHandle = m_instances.Add(instanceData);

	const Uint32 idx = instanceHandle.idx;

	const math::Matrix4f& transformMatrix = def.transform.matrix();

	const Real32 scaleX2 = transformMatrix.row(0).head<3>().squaredNorm();
	const Real32 scaleY2 = transformMatrix.row(1).head<3>().squaredNorm();
	const Real32 scaleZ2 = transformMatrix.row(2).head<3>().squaredNorm();

	const Real32 maxScale = std::max(std::max(scaleX2, scaleY2), scaleZ2);
	const Real32 uniformScale = std::sqrt(maxScale);

	RenderEntityGPUData entityGPUData;
	entityGPUData.transform    = transformMatrix;
	entityGPUData.uniformScale = uniformScale;

	const Byte* entityDataPtr = reinterpret_cast<const Byte*>(&entityGPUData);
	rdr::UploadDataToBuffer(m_renderEntitiesBuffer, idx * sizeof(RenderEntityGPUData), entityDataPtr, sizeof(RenderEntityGPUData));

	return instanceHandle;
}

const lib::SharedRef<rdr::Buffer>& RenderScene::GetRenderEntitiesBuffer() const
{
	return m_renderEntitiesBuffer;
}

void RenderScene::SetTerrainDefinition(const TerrainDefinition& definition)
{
	m_terrainDefinition = definition;
}

const TerrainDefinition& RenderScene::GetTerrainDefinition() const
{
	return m_terrainDefinition;
}

lib::SharedRef<rdr::Buffer> RenderScene::CreateInstancesBuffer() const
{
	rhi::RHIAllocationInfo renderEntitiesAllocationInfo;
	renderEntitiesAllocationInfo.memoryUsage = rhi::EMemoryUsage::GPUOnly;

	const Uint64 maxInstancesNum = 8192u;

	rhi::BufferDefinition renderEntitiesBufferDef;
	renderEntitiesBufferDef.size = maxInstancesNum * sizeof(RenderEntityGPUData);
	renderEntitiesBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	renderEntitiesBufferDef.flags = rhi::EBufferFlags::WithVirtualSuballocations;

	return rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("RenderEntitiesGPUDataBuffer"), renderEntitiesBufferDef, renderEntitiesAllocationInfo);
}

} // spt::rsc
