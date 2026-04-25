#include "SculptorShader.hlsli"

[[shader_params(ApplyPOMOffsetConstants, u_constants)]]

[[descriptor_set(RenderViewDS)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/FullScreen.hlsli"


struct PS_OUTPUT
{
	float depth : SV_DepthLessEqual;
};


PS_OUTPUT ApplyPOMOffsetFS(VS_OUTPUT input)
{
	const int2 coords = input.uv * u_constants.resolution;

	const float depth = u_constants.depth.Load(coords);
	const float linearDepth = ComputeLinearDepth(depth, u_sceneView);

	const float pomDepth = u_constants.pomDepth.Load(coords) * 0.01f * POM_MAX_DEPTH_OFFSET_CM;

	const float finalDepth = linearDepth + pomDepth;

	PS_OUTPUT output;
	output.depth = GetNearPlane(u_sceneView) / finalDepth;

	return output;
}
