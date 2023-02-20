#include "StaticMeshShadowMapRenderer.h"
#include "RenderScene.h"
#include "Shadows/ShadowMapsManagerSystem.h"
#include "Lights/LightTypes.h"
#include "RenderGraphBuilder.h"
#include "Transfers/UploadUtils.h"

namespace spt::rsc
{

StaticMeshShadowMapRenderer::StaticMeshShadowMapRenderer()
{

}

void StaticMeshShadowMapRenderer::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	m_pointLightBatches.clear();

	if (const ShadowMapsManagerSystem* shadowMapsManager = renderScene.GetPrimitivesSystem<ShadowMapsManagerSystem>())
	{
		const RenderSceneRegistry& sceneRegistry = renderScene.GetRegistry();

		const lib::DynamicArray<RenderSceneEntity>& pointLightsToUpdate = shadowMapsManager->GetPointLightsWithShadowMapsToUpdate();

		m_pointLightBatches.reserve(pointLightsToUpdate.size());

		for (const RenderSceneEntity pointLight : pointLightsToUpdate)
		{
			const PointLightData& pointLightData = sceneRegistry.get<PointLightData>(pointLight);
			const PointLightShadowMapComponent& pointLightShadowMap = sceneRegistry.get<PointLightShadowMapComponent>(pointLight);

			const StaticMeshPrimitivesSystem& staticMeshPrimsSystem = renderScene.GetPrimitivesSystemChecked<StaticMeshPrimitivesSystem>();
			const StaticMeshBatchDefinition batchDef = staticMeshPrimsSystem.BuildBatchForPointLight(pointLightData);

			const lib::DynamicArray<RenderView*> shadowMapViews = shadowMapsManager->GetPointLightShadowMapViews(pointLightShadowMap);
			
			m_pointLightBatches.emplace(pointLight, CreateBatch(graphBuilder, renderScene, shadowMapViews, batchDef));
		}
	}
}

SMShadowMapBatch StaticMeshShadowMapRenderer::CreateBatch(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<RenderView*>& batchedViews, const StaticMeshBatchDefinition& batchDef) const
{
	SPT_PROFILER_FUNCTION();

	SMShadowMapBatch batch;

	// Create batch data and buffer

	SMGPUBatchData gpuBatchData;
	gpuBatchData.elementsNum = static_cast<Uint32>(batchDef.batchElements.size());

	const Uint64 batchDataSize = sizeof(StaticMeshBatchElement) * batchDef.batchElements.size();
	const rhi::BufferDefinition batchBufferDef(batchDataSize, rhi::EBufferUsage::Storage);
	const rhi::RHIAllocationInfo batchBufferAllocation(rhi::EMemoryUsage::CPUToGPU);
	const lib::SharedRef<rdr::Buffer> batchBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("StaticMeshesBatch"), batchBufferDef, batchBufferAllocation);

	gfx::UploadDataToBuffer(batchBuffer, 0, reinterpret_cast<const Byte*>(batchDef.batchElements.data()), batchDataSize);

	batch.batchDS = rdr::ResourcesManager::CreateDescriptorSetState<StaticMeshBatchDS>(RENDERER_RESOURCE_NAME("StaticMeshesBatchDS"));
	batch.batchDS->u_batchElements	= batchBuffer->CreateFullView();
	batch.batchDS->u_batchData		= gpuBatchData;

	// Create indirect draw counts buffer

	const Uint64 indirectDrawCountsBuffersSize = batchedViews.size() * sizeof(Uint32);
	rhi::BufferDefinition bufferDef(indirectDrawCountsBuffersSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect, rhi::EBufferUsage::TransferDst));
	const rg::RGBufferViewHandle indirectDrawCountsBuffer= graphBuilder.CreateBufferView(RG_DEBUG_NAME("IndirectDrawCountsPerSide"), bufferDef, rhi::EMemoryUsage::GPUOnly);

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Initialize IndirectDrawCountsBuffer"), indirectDrawCountsBuffer, 0, indirectDrawCountsBuffersSize, 0);

	batch.indirectDrawCountsBuffer = indirectDrawCountsBuffer;

	// Create culling resources per shadow map face

	const rhi::BufferDefinition commandsBufferDef(sizeof(SMDepthOnlyDrawCallData) * batchDef.batchElements.size(), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::Indirect));

	batch.perFaceData.reserve(batchedViews.size());

	for (SizeType faceIdx = 0; faceIdx < batchedViews.size(); ++faceIdx)
	{
		SMShadowMapBatch::PerFaceData faceData;

		const rg::RGBufferViewHandle drawCommandsBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("ShadowMapDrawCommandsBuffer"), commandsBufferDef, rhi::EMemoryUsage::GPUOnly);

		SMShadowMapDataPerView shadowMapDataPerView;
		shadowMapDataPerView.faceIdx = static_cast<Uint32>(faceIdx);

		const lib::SharedRef<SMShadowMapCullingDS> cullingDS = rdr::ResourcesManager::CreateDescriptorSetState<SMShadowMapCullingDS>(RENDERER_RESOURCE_NAME("SMShadowMapCullingDS"));
		cullingDS->u_cullingData		= batchedViews[faceIdx]->GetCullingData();
		cullingDS->u_shadowMapViewData	= shadowMapDataPerView;
		cullingDS->u_drawCommands		= drawCommandsBuffer;
		cullingDS->u_drawsCount			= indirectDrawCountsBuffer;

		const lib::SharedRef<SMDepthOnlyDrawInstancesDS> drawDS = rdr::ResourcesManager::CreateDescriptorSetState<SMDepthOnlyDrawInstancesDS>(RENDERER_RESOURCE_NAME("SMShadowMapDrawInstancesDS"));
		drawDS->u_drawCommands = drawCommandsBuffer;

		faceData.cullingDS	= cullingDS;
		faceData.drawDS		= drawDS;

		batch.perFaceData.emplace_back(faceData);
	}

	return batch;
}

} // spt::rsc
