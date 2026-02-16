#ifdef GEOMETRY_PIPELINE_DEFINITIONS_PASS
[[descriptor_set(VisBufferRenderingDS)]]

struct CustomMeshletsData
{
	uint firstVisibleMeshletIdx;
};
#define GEOMETRY_PIPELINE_CUSTOM_MESHLETS_DATA_TYPE CustomMeshletsData

#define GEOMETRY_PIPELINE_CUSTOM_PER_PRIMITIVE_DATA \
	uint packedVisibilityInfo : PACKED_VISBILITY_INFO;

#else

void DispatchMeshlet(in MeshletDispatchContext context, out CustomMeshletsData customMeshletsData)
{
	if(context.isVisible)
	{
		const uint visibleMeshletsNumInWave = WaveActiveCountBits(context.isVisible);
		uint visibleMeshletIdx = 0u;
		if (WaveIsFirstLane())
		{
			InterlockedAdd(u_visibleMeshletsCount[0], visibleMeshletsNumInWave, visibleMeshletIdx);
			customMeshletsData.firstVisibleMeshletIdx = visibleMeshletIdx;
		}
		visibleMeshletIdx = WaveReadLaneFirst(visibleMeshletIdx) + context.compactedLocalVisibleIdx;

		GPUVisibleMeshlet visibleMeshletInfo;
		visibleMeshletInfo.entityPtr          = context.batchElement.entityPtr;
		visibleMeshletInfo.submeshPtr         = context.batchElement.submeshPtr;
		visibleMeshletInfo.meshletPtr         = context.submesh.meshlets.GetElemPtr(context.localMeshletIdx);
		visibleMeshletInfo.materialDataHandle = context.batchElement.materialDataHandle;
		visibleMeshletInfo.materialBatchIdx   = context.batchElement.materialBatchIdx;

		u_visibleMeshlets[visibleMeshletIdx] = visibleMeshletInfo;
	}
}


void DispatchTriangle(in TriangleDispatchContext context, in CustomMeshletsData customMeshletsData, inout PerPrimitiveData primData)
{
	// each group of mesh shader is processing different meshlet. Those are already compacted based on visibility
	// e.g. second group is processing second visible meshlet 
	// it doesn't necessarily mean that it's processing second meshlet in the submesh, because some previous meshlets might be culled out
	const uint visibleMeshletIdx = customMeshletsData.firstVisibleMeshletIdx + context.groupID;
	primData.packedVisibilityInfo = PackVisibilityInfo(visibleMeshletIdx, context.meshletTriangleIdx);
}


struct VIS_BUFFER_PS_OUT
{
	uint packedVisibilityInfo : SV_TARGET0;
};


#define FRAGMENT_SHADER_OUTPUT_TYPE VIS_BUFFER_PS_OUT


FRAGMENT_SHADER_OUTPUT_TYPE DispatchFragment(in FragmentDispatchContext context)
{
	VIS_BUFFER_PS_OUT output;
	output.packedVisibilityInfo = context.primData.packedVisibilityInfo;
	return output;
}
#endif // GEOMETRY_PIPELINE_DEFINITIONS_PASS
