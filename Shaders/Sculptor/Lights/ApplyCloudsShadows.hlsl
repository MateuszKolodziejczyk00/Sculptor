#include "SculptorShader.hlsli"

[[descriptor_set(ApplyCloudsShadowsDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ApplyCloudsShadowsCS(CS_INPUT input)
{
    const float2 uv = (input.globalID.xy + 0.5f) * u_constants.rcpResolution;

    const float depth = u_depth.Load(uint3(input.globalID.xy, 0u));

    const float3 ndc = float3(uv * 2.f - 1.f, depth);

    const float3 worldSpace = NDCToWorldSpace(ndc, u_sceneView);

	const float4 ctmCS = mul(u_constants.viewProjectionMatrix, float4(worldSpace, 1.f));

    float cloudsTransmittance = 1.f;
	if(all(ctmCS.xy <= ctmCS.w) && all(ctmCS.xy >= -ctmCS.w))
	{
		const float2 ctmUV = (ctmCS.xy / ctmCS.w) *	0.5f + 0.5f;

		cloudsTransmittance = u_cloudsTransmittanceMap.SampleLevel(u_linearSampler, ctmUV, 0.f);
	}

    u_shadowMask[input.globalID.xy] = u_shadowMask[input.globalID.xy] * cloudsTransmittance;
}