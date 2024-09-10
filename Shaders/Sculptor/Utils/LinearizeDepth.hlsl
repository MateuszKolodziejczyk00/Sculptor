#include "SculptorShader.hlsli"

[[descriptor_set(LinearizeDepthDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 localID  : SV_GroupThreadID;
};


#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8


#define CACHE_COHERENT_WRITES 1


groupshared float gs_linearDepth[GROUP_SIZE_X * 2][GROUP_SIZE_Y * 2];


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void LinearizeDepthCS(CS_INPUT input)
{
	const uint2 quadCoords = input.globalID.xy * 2u;

	const float2 uv = (quadCoords + 1.f) * u_constants.invResolution;
	const float4 depthSample = u_depth.Gather(u_nearestSampler, uv);

	const float4 linearDepth = ComputeLinearDepth(depthSample, u_sceneView);

	const uint2 lastCoord = u_constants.resolution - 1;

#if CACHE_COHERENT_WRITES

	const uint2 localQuadID = input.localID.xy * 2u;

	gs_linearDepth[localQuadID.x + 0][localQuadID.y + 0] = linearDepth.w;
	gs_linearDepth[localQuadID.x + 1][localQuadID.y + 0] = linearDepth.z;
	gs_linearDepth[localQuadID.x + 1][localQuadID.y + 1] = linearDepth.y;
	gs_linearDepth[localQuadID.x + 0][localQuadID.y + 1] = linearDepth.x;

	GroupMemoryBarrierWithGroupSync();

	const uint2 groupOutputCoords = quadCoords - localQuadID;

	const uint2 localOutputCoords = groupOutputCoords + input.localID.xy;

	u_rwLinearDepth[min(localOutputCoords, lastCoord)]                                     = gs_linearDepth[input.localID.x][input.localID.y];
	u_rwLinearDepth[min(localOutputCoords + uint2(GROUP_SIZE_X, 0), lastCoord)]            = gs_linearDepth[input.localID.x + GROUP_SIZE_X][input.localID.y];
	u_rwLinearDepth[min(localOutputCoords + uint2(0, GROUP_SIZE_Y), lastCoord)]            = gs_linearDepth[input.localID.x][input.localID.y + GROUP_SIZE_Y];
	u_rwLinearDepth[min(localOutputCoords + uint2(GROUP_SIZE_X, GROUP_SIZE_Y), lastCoord)] = gs_linearDepth[input.localID.x + GROUP_SIZE_X][input.localID.y + GROUP_SIZE_Y];

#else

	u_rwLinearDepth[min(quadCoords, lastCoord)]               = linearDepth.w;
	u_rwLinearDepth[min(quadCoords + uint2(1, 0), lastCoord)] = linearDepth.z;
	u_rwLinearDepth[min(quadCoords + uint2(1, 1), lastCoord)] = linearDepth.y;
	u_rwLinearDepth[min(quadCoords + uint2(0, 1), lastCoord)] = linearDepth.x;

#endif // CACHE_COHERENT_WRITES
}
