#ifndef GEOMETRY_COMMON_H
#define GEOMETRY_COMMON_H

#define SPT_GEOMETRY_VISIBLE_GEOMETRY_PASS     0
#define SPT_GEOMETRY_DISOCCLUDED_GEOMETRY_PASS 1
#define SPT_GEOMETRY_DISOCCLUDED_MESHLETS_PASS 2


uint PackVisibilityInfo(in uint visibleMeshletIdx, in uint triangleIdx)
{
	SPT_CHECK_MSG(triangleIdx < (1 << 7), L"Invalid triangle index - {}", triangleIdx);
	return (visibleMeshletIdx << 7) | triangleIdx;
}


bool UnpackVisibilityInfo(in uint packedInfo, out uint visibleMeshletIdx, out uint triangleIdx)
{
	visibleMeshletIdx = packedInfo >> 7;
	triangleIdx = packedInfo & 0x7F;
	return (visibleMeshletIdx != (~0u >> 7));
}


float MaterialBatchIdxToMaterialDepth(in uint materialBatchIdx)
{
	return materialBatchIdx / float(0xFFFF);
}


float MaterialDepthToMaterialBatchIdx(in float materialDepth)
{
	return materialDepth * float(0xFFFF);
}

#endif // GEOMETRY_COMMON_H