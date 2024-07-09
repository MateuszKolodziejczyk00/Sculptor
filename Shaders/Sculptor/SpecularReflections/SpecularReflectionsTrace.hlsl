#include "SculptorShader.hlsli"

#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/SpecularReflectionsTracingCommon.hlsli"
#include "SpecularReflections/RTGBuffer.hlsli"

#include "Utils/SceneViewUtils.hlsli"

#include "Utils/RayBinning/RayBinning.hlsli"


RayHitResult TraceReflectionRay(in float3 rayOrigin, in float3 rayDirection)
{
	const float maxHitDistance = 200.f;

	RayDesc rayDesc;
	rayDesc.TMin      = 0.004f;
	rayDesc.TMax      = maxHitDistance;
	rayDesc.Origin    = rayOrigin;
	rayDesc.Direction = rayDirection;

	SpecularReflectionsRayPayload payload;

	TraceRay(u_sceneTLAS,
			 0,
			 0xFF,
			 0,
			 1,
			 0,
			 rayDesc,
			 payload);

	const bool isBackface = payload.distance < 0.f;
	const bool isValidHit = !isBackface && payload.distance < maxHitDistance;

	RayHitResult hitResult = RayHitResult::CreateEmpty();

	if (isValidHit)
	{
		const float4 baseColorMetallic = UnpackFloat4x8(payload.baseColorMetallic);

		hitResult.normal      = normalize(payload.normal);
		hitResult.roughness   = payload.roughness;
		hitResult.baseColor   = baseColorMetallic.rgb;
		hitResult.metallic    = baseColorMetallic.w;
		hitResult.hitType     = RTGBUFFER_HIT_TYPE_VALID_HIT;
		hitResult.hitDistance = payload.distance;
	}
	else if (!isBackface)
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_NO_HIT;
	}
	else
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_BACKFACE;
	}

	return hitResult;
}


void ReorderThreads(inout uint2 threadID, Texture2D<uint> reorderingsTexture)
{
	ExecuteReordering(INOUT threadID, reorderingsTexture);
}


[shader("raygeneration")]
void GenerateSpecularReflectionsRaysRTG()
{
	uint2 pixel = DispatchRaysIndex().xy;

	ReorderThreads(INOUT pixel, u_reorderingsTexture);

	const float depth = u_depthTexture.Load(uint3(pixel, 0));
	if (depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) / float2(DispatchRaysDimensions().xy);
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

		const float2 encodedRayDirection = u_rayDirectionsTexture.Load(uint3(pixel, 0)).xy;

		const float3 rayDirection = OctahedronDecodeNormal(encodedRayDirection);

		const RayHitResult hitResult = TraceReflectionRay(worldLocation, rayDirection);

		const RTGBufferData gBufferData = PackRTGBuffer(hitResult);

		RTGBuffer<RWTexture2D> rtgBuffer;
		rtgBuffer.hitNormal            = u_hitNormalTexture;
		rtgBuffer.hitBaseColorMetallic = u_hitBaseColorMetallicTexture;
		rtgBuffer.hitRoughness         = u_hitRoughnessTexture;
		rtgBuffer.rayDistance          = u_rayDistanceTexture;

		WriteRTGBuffer(pixel, gBufferData, rtgBuffer);
	}
}

[shader("miss")]
void SpecularReflectionsRTM(inout SpecularReflectionsRayPayload payload)
{
	payload.distance = 999999.f;
}
