#include "SculptorShader.hlsli"

[[descriptor_set(ReprojectVariableRateTextureDS, 0)]]

#if USE_DEPTH_TEST
[[descriptor_set(RenderViewDS, 1)]]
#endif // USE_DEPTH_TEST

#include "Utils/VariableRate/VariableRate.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Shapes.hlsli"
#include "Utils/Packing.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


float2 SampleMotionTexture(in uint2 pixel)
{
	float2 motion = 0.f;
	float maxMotionSquared = 0.f;

	for(uint y = 0; y < 2; ++ y)
	{
		for(uint x = 0; x < 2; ++ x)
		{
			const float2 sampleMotion = u_motionTexture.Load(uint3(pixel * 2 + uint2(x, y), 0)).xy;
			const float sampleMotionSquared = dot(sampleMotion, sampleMotion);
			if(sampleMotionSquared > maxMotionSquared)
			{
				motion = sampleMotion;
				maxMotionSquared = sampleMotionSquared;
			}
		}
	}

	return motion;
}


uint GetReprojectionFailedVariableRate()
{
	if (u_constants.reprojectionFailedMode == 0)
	{
		return SPT_VARIABLE_RATE_1X1;
	}
	else if (u_constants.reprojectionFailedMode == 1)
	{
		return SPT_VARIABLE_RATE_2Y;
	}
	else if (u_constants.reprojectionFailedMode == 2)
	{
		return SPT_VARIABLE_RATE_2X2;
	}
#if SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
	else if (u_constants.reprojectionFailedMode == 3)
	{
		return SPT_VARIABLE_RATE_4X4;
	}
#endif // SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4

	SPT_CHECK_MSG(false, L"Invalid reprojection failed mode");
	return SPT_VARIABLE_RATE_1X1;
}


struct VariableRateReprojectionResult
{
	bool isSuccessful;
	uint variableRateMask;
};


bool CanUseReprojection(in uint2 coords, in float2 uv, in uint2 reprojectedCoords, in float2 reprojectedUV)
{
	bool canUseReprojection = true;

#if USE_DEPTH_TEST
	const float depth = u_depthTexture.Load(uint3(coords, 0u));

	if(depth > 0.f)
	{
		const float3 currentWS = NDCToWorldSpace(float3(uv * 2.f - 1.f, depth), u_sceneView);
		const float3 normal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(coords, 0u)).xy);
		const Plane plane = Plane::Create(normal, currentWS);

		const float reprojectedDepth = u_historyDepthTexture.Load(uint3(reprojectedCoords, 0u));
		const float3 reprojectedWS = NDCToWorldSpace(float3(reprojectedUV * 2.f - 1.f, reprojectedDepth), u_prevFrameSceneView);

		canUseReprojection = canUseReprojection && plane.Distance(reprojectedWS) < u_constants.reprojectionMaxPlaneDistance;
	}
#endif // USE_DEPTH_TEST

	return canUseReprojection;
}


VariableRateReprojectionResult TryReproject(in uint2 coords, in float2 uv, in float2 motion)
{
	VariableRateReprojectionResult result;
	result.isSuccessful = false;

	const float2 reprojectedUV = uv - motion;

	if(all(reprojectedUV >= 0.f) && all(reprojectedUV <= 1.f))
	{
		const uint2 reprojectedCoords = uint2(reprojectedUV * u_constants.resolution);

		// depth targets have 2x higher res than variable rate so we need to shift the coords
		if (CanUseReprojection(coords << 1u, uv, reprojectedCoords << 1u, reprojectedUV))
		{
			const bool hasNoMotion = IsNearlyZero(motion.x) && IsNearlyZero(motion.y);
			const uint variableRate = hasNoMotion 
									? LoadHistoryVariableRate(u_inputTexture, reprojectedCoords)
									: LoadHistoryVariableRateSubsampled(u_inputTexture, reprojectedCoords);

			result.isSuccessful     = true;
			result.variableRateMask = variableRate;
		}
	}

	return result;
}



[numthreads(8 , 4 , 1)]
void ReprojectVariableRateTextureCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;

	const float2 uv = (pixel + 0.5f) * u_constants.invResolution;

	const float2 motion = SampleMotionTexture(pixel);

	const VariableRateReprojectionResult reprojectionResult = TryReproject(pixel, uv, motion);
	const uint variableRate = reprojectionResult.isSuccessful ? reprojectionResult.variableRateMask : CreateCompressedVariableRateData(GetReprojectionFailedVariableRate());

	u_rwOutputTexture[pixel] = variableRate;

#if OUTPUT_REPROJECTION_SUCCESS_MASK

	const uint2 maskCoords = pixel / uint2(8u, 4u);

	uint successMask = reprojectionResult.isSuccessful ? 1u << WaveGetLaneIndex() : 0;
	successMask = WaveActiveBitOr(successMask);

	u_rwReprojectionSuccessMask[maskCoords] = successMask;

#endif // OUTPUT_REPROJECTION_SUCCESS_MASK
}
