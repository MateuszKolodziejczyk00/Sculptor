#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(ApplyCloudsShadowsConstants, CONSTS)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ApplyCloudsShadowsCS(CS_INPUT input)
{
	const float2 uv = (input.globalID.xy + 0.5f) * CONSTS->rcpResolution;

	const float depth = CONSTS->depth.Load(uint3(input.globalID.xy, 0u));

	if (depth == 0.f)
	{
		return;
	}

	const float3 ndc = float3(uv * 2.f - 1.f, depth);

	const float3 worldSpace = NDCToWorldSpace(ndc, VIEW->sceneView);

	const float4 ctmCS = mul(CONSTS->viewProjectionMatrix, float4(worldSpace, 1.f));

	float cloudsTransmittance = 1.f;
	if(all(ctmCS.xy <= ctmCS.w) && all(ctmCS.xy >= -ctmCS.w))
	{
		const float2 ctmUV = (ctmCS.xy / ctmCS.w) *	0.5f + 0.5f;

		cloudsTransmittance = CONSTS->cloudsTransmittanceMap.SampleLevel(BindlessSamplers::LinearClampEdge(), ctmUV, 0.f);
	}

	CONSTS->shadowMask[input.globalID.xy] = CONSTS->shadowMask[input.globalID.xy] * cloudsTransmittance;
}
