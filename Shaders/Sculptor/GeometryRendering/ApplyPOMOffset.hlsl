#include "SculptorShader.hlsli"

[[shader_params(ApplyPOMOffsetConstants, PARAMS_APPLY_P_O_M_OFFSET_CONSTANTS)]]

[[shader_params(GPURenderView, VIEW)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/FullScreen.hlsli"


struct PS_OUTPUT
{
	float depth : SV_DepthLessEqual;
};


PS_OUTPUT ApplyPOMOffsetFS(VS_OUTPUT input)
{
	const int2 coords = input.uv * PARAMS_APPLY_P_O_M_OFFSET_CONSTANTS->resolution;

	const float depth = PARAMS_APPLY_P_O_M_OFFSET_CONSTANTS->depth.Load(coords);
	const float linearDepth = ComputeLinearDepth(depth, VIEW->sceneView);

	const float pomDepth = PARAMS_APPLY_P_O_M_OFFSET_CONSTANTS->pomDepth.Load(coords) * 0.01f * POM_MAX_DEPTH_OFFSET_CM;

	const float finalDepth = linearDepth + pomDepth;

	PS_OUTPUT output;
	output.depth = GetNearPlane(VIEW->sceneView) / finalDepth;

	return output;
}
