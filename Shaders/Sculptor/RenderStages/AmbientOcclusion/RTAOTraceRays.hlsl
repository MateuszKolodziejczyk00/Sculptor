#include "SculptorShader.hlsli"

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(GPURenderView, VIEW)]]
[[shader_params(RTAOTraceRaysParams, PARAMS_R_T_A_O_TRACE_RAYS)]]

#include "RayTracing/RayTracingHelpers.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/Random.hlsli"


[shader("raygeneration")]
void GenerateAmbientOcclusionRaysRTG()
{
    const uint2 pixel = DispatchRaysIndex().xy;

    const float2 uv = (pixel + 0.5f) / float2(DispatchRaysDimensions().xy);
    const float depth = PARAMS_R_T_A_O_TRACE_RAYS->depthTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0);
	const float linearDepth = ComputeLinearDepth(depth, VIEW->sceneView);

    float ao = 1.f;

    const float aoRange = 100.f;
    
    if(depth > 0.f && linearDepth < aoRange)
    {
        const float3 ndc = float3(uv * 2.f - 1.f, depth);
        const float3 worldLocation = NDCToWorldSpace(ndc, VIEW->sceneView);

        const float3 normal = OctahedronDecodeNormal(PARAMS_R_T_A_O_TRACE_RAYS->normalsTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0));
        const float3 tangent = abs(dot(normal, UP_VECTOR)) > 0.9f ? cross(normal, RIGHT_VECTOR) : cross(normal, UP_VECTOR);
        const float3 bitangent = cross(normal, tangent);

        const float2 random = float2(Random(float2(pixel) + PARAMS_R_T_A_O_TRACE_RAYS->randomSeed.xy), Random(float2(pixel.yx) + PARAMS_R_T_A_O_TRACE_RAYS->randomSeed.yx));

        const float3x3 tangentSpace = transpose(float3x3(tangent, bitangent, normal));

        float pdf = 0.f;
        const float3 rayDirection = RandomVectorInCosineWeightedHemisphere(tangentSpace, random, OUT pdf);

        RayDesc rayDesc;
        rayDesc.TMin        = PARAMS_R_T_A_O_TRACE_RAYS->raysMinHitDistance;
        rayDesc.TMax        = PARAMS_R_T_A_O_TRACE_RAYS->raysLength;
        rayDesc.Origin      = worldLocation + normal * 0.02f;
        rayDesc.Direction   = rayDirection;

		ao = RTScene().VisibilityTest(rayDesc) ? 1.f : 0.f;
	}

    PARAMS_R_T_A_O_TRACE_RAYS->ambientOcclusionTexture[pixel] = ao;
}
