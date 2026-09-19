#include "SculptorShader.hlsli"

[[shader_params(TileMinDepthConstants, PARAMS_RENDER_TILE_MIN_DEPTH)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


[numthreads(8, 4, 1)]
void TileMinDepthCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy * 2;
	const float2 uv   = (float2(pixel) + 1.f) * PARAMS_RENDER_TILE_MIN_DEPTH->depthInvRes; // add 1 because we sample 2x2 squares using min filter

	const float minDepth2x2 = PARAMS_RENDER_TILE_MIN_DEPTH->depthTexture.SampleLevel(BindlessSamplers::LinearMinClampEdge(), uv, 0).x;

	const float leftTileDepth = input.localID.x < 4  ? minDepth2x2 : 999999.f;
	const float rightTileSize = input.localID.x >= 4 ? minDepth2x2 : 999999.f;

	const float2 tilesMinDepth = WaveActiveMin(float2(leftTileDepth, rightTileSize));

	const uint2 outputTile = input.groupID.xy * uint2(2, 1);

	const uint threadIdx = WaveGetLaneIndex();
	if (threadIdx < 2)
	{
		PARAMS_RENDER_TILE_MIN_DEPTH->outMinDepthTexture[outputTile + uint2(threadIdx, 0)] = tilesMinDepth[threadIdx];
	}
}
