#include "SculptorShader.hlsli"

[[shader_params(ApplyCloudsShadowsConstants, PARAMS_APPLY_CLOUDS_SHADOWS)]]
[[shader_params(GPURenderView, VIEW)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ApplyCloudsShadowsCS(CS_INPUT input)
{
	const float2 uv = (input.globalID.xy + 0.5f) * PARAMS_APPLY_CLOUDS_SHADOWS->rcpResolution;

	const float depth = PARAMS_APPLY_CLOUDS_SHADOWS->depth.Load(uint3(input.globalID.xy, 0u));

	const float3 ndc = float3(uv * 2.f - 1.f, depth);

	const float3 worldSpace = NDCToWorldSpace(ndc, VIEW->sceneView);

	const float4 ctmCS = mul(PARAMS_APPLY_CLOUDS_SHADOWS->viewProjectionMatrix, float4(worldSpace, 1.f));

	float cloudsTransmittance = 1.f;
	if(all(ctmCS.xy <= ctmCS.w) && all(ctmCS.xy >= -ctmCS.w))
	{
		const float2 ctmUV = (ctmCS.xy / ctmCS.w) *	0.5f + 0.5f;

		cloudsTransmittance = PARAMS_APPLY_CLOUDS_SHADOWS->cloudsTransmittanceMap.SampleLevel(BindlessSamplers::LinearClampEdge(), ctmUV, 0.f);
	}

	PARAMS_APPLY_CLOUDS_SHADOWS->shadowMask[input.globalID.xy] = PARAMS_APPLY_CLOUDS_SHADOWS->shadowMask[input.globalID.xy] * cloudsTransmittance;
}