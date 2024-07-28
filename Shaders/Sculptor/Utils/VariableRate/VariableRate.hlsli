#ifndef VARIABLE_RATE_HLSLI
#define VARIABLE_RATE_HLSLI

#define SPT_VARIABLE_RATE_MODE_2X2 0
#define SPT_VARIABLE_RATE_MODE_4X4 1

#ifndef SPT_VARIABLE_RATE_MODE
#define SPT_VARIABLE_RATE_MODE SPT_VARIABLE_RATE_MODE_2X2
#endif // SPT_VARIABLE_RATE_MODE

#define SPT_VARIABLE_RATE_2X (1 << 0)
#define SPT_VARIABLE_RATE_2Y (1 << 1)

#if SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
	#define SPT_VARIABLE_RATE_4X (1 << 2)
	#define SPT_VARIABLE_RATE_4Y (1 << 3)
#endif // SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4

#if SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_2X2
	#define SPT_VARIABLE_RATE_BITS 2
#elif SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
	#define SPT_VARIABLE_RATE_BITS 4
#endif // SPT_VARIABLE_RATE_MODE
#define SPT_VARIABLE_RATE_MASK ((1 << SPT_VARIABLE_RATE_BITS) - 1)

#define SPT_VARIABLE_RATE_1X1	(0)
#define SPT_VARIABLE_RATE_2X2	(SPT_VARIABLE_RATE_2X | SPT_VARIABLE_RATE_2Y)

#if SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
	#define SPT_VARIABLE_RATE_4X4	(SPT_VARIABLE_RATE_4X | SPT_VARIABLE_RATE_4Y)
#endif // SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4

#define SPT_VARIABLE_RATE_2X2_MASK	SPT_VARIABLE_RATE_2X2
#if SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
	#define SPT_VARIABLE_RATE_4X4_MASK	SPT_VARIABLE_RATE_4X4
#endif // SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4

#if SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_2X2
	#define SPT_VARIABLE_RATE_X_MASK (SPT_VARIABLE_RATE_2X)
	#define SPT_VARIABLE_RATE_Y_MASK (SPT_VARIABLE_RATE_2Y)
#elif SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
	#define SPT_VARIABLE_RATE_X_MASK (SPT_VARIABLE_RATE_2X | SPT_VARIABLE_RATE_4X)
	#define SPT_VARIABLE_RATE_Y_MASK (SPT_VARIABLE_RATE_2Y | SPT_VARIABLE_RATE_4Y)
#endif // SPT_VARIABLE_RATE_MODE

#if SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_2X2
	#define SPT_MAX_VARIABLE_RATE SPT_VARIABLE_RATE_2X2
#elif SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
	#define SPT_MAX_VARIABLE_RATE SPT_VARIABLE_RATE_4X4
#endif // SPT_VARIABLE_RATE_MODE

#define SPT_VARIABLE_RATE_TILE_2x2 (0)
#define SPT_VARIABLE_RATE_TILE_4x4 (1)


uint2 GetVariableRateTileSize(in uint variableRateMask)
{
	uint2 tileSize = 1;
	tileSize.x = (variableRateMask & SPT_VARIABLE_RATE_2X) ? 2 : tileSize.x;
	tileSize.y = (variableRateMask & SPT_VARIABLE_RATE_2Y) ? 2 : tileSize.y;
#if SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
	tileSize.x = (variableRateMask & SPT_VARIABLE_RATE_4X) ? 4 : tileSize.x;
	tileSize.y = (variableRateMask & SPT_VARIABLE_RATE_4Y) ? 4 : tileSize.y;
#endif // SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
	return tileSize;
}


uint ClampTo2x2VariableRate(in uint variableRateMask)
{
	uint clampedMask = variableRateMask;

#if SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
	if((clampedMask & SPT_VARIABLE_RATE_4X) > 0)
	{
		clampedMask &= ~SPT_VARIABLE_RATE_4X;
		clampedMask |= SPT_VARIABLE_RATE_2X;
	}

	if((clampedMask & SPT_VARIABLE_RATE_4Y) > 0)
	{
		clampedMask &= ~SPT_VARIABLE_RATE_4Y;
		clampedMask |= SPT_VARIABLE_RATE_2Y;
	}
#endif // SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4

	return clampedMask;
}


uint MinVariableRate(in uint a, in uint b)
{
	const uint ax = a & SPT_VARIABLE_RATE_X_MASK;
	const uint ay = a & SPT_VARIABLE_RATE_Y_MASK;

	const uint bx = b & SPT_VARIABLE_RATE_X_MASK;
	const uint by = b & SPT_VARIABLE_RATE_Y_MASK;

	return min(ax, bx) | min(ay, by);
}

uint DecompressVariableRateFromTexture(in uint variableRateData)
{
	uint decompressedMask = SPT_MAX_VARIABLE_RATE;
	decompressedMask = MinVariableRate(decompressedMask, (variableRateData >> (SPT_VARIABLE_RATE_BITS * 0)) & SPT_VARIABLE_RATE_MASK);
	decompressedMask = MinVariableRate(decompressedMask, (variableRateData >> (SPT_VARIABLE_RATE_BITS * 1)) & SPT_VARIABLE_RATE_MASK);
	decompressedMask = MinVariableRate(decompressedMask, (variableRateData >> (SPT_VARIABLE_RATE_BITS * 2)) & SPT_VARIABLE_RATE_MASK);
	decompressedMask = MinVariableRate(decompressedMask, (variableRateData >> (SPT_VARIABLE_RATE_BITS * 3)) & SPT_VARIABLE_RATE_MASK);

	return decompressedMask;
}

uint LoadVariableRate(in Texture2D<uint> vrTexture, in uint2 pixel)
{
	const uint variableRateData = vrTexture.Load(uint3(pixel, 0));

	return DecompressVariableRateFromTexture(variableRateData);
}


uint ReprojectVariableRate(in Texture2D<uint> vrTexture, in float2 uv, in uint2 variableRateRes, in float2 motion)
{
	const float2 reprojectedUV = uv - motion;

	uint variableRateMask = SPT_VARIABLE_RATE_1X1;

	if(all(reprojectedUV >= 0.f) && all(reprojectedUV <= 1.f))
	{
		if(IsNearlyZero(motion.x) && IsNearlyZero(motion.y))
		{
			variableRateMask = vrTexture.Load(uint3(uv * variableRateRes, 0));
		}
		else
		{
			const uint2 reprojectedCoords = reprojectedUV * variableRateRes;

			uint4 samples = 0.f;
			samples.x = vrTexture.Load(uint3(reprojectedCoords + uint2(0, 0), 0));
			samples.y = vrTexture.Load(uint3(reprojectedCoords + uint2(0, 1), 0));
			samples.z = vrTexture.Load(uint3(reprojectedCoords + uint2(1, 0), 0));
			samples.w = vrTexture.Load(uint3(reprojectedCoords + uint2(1, 1), 0));

			variableRateMask = 0;
			
			[unroll]
			for(uint i = 0; i < 4; ++i)
			{
				const uint offset = i * SPT_VARIABLE_RATE_BITS;

				uint vrSample = SPT_MAX_VARIABLE_RATE;
				vrSample = MinVariableRate(vrSample, (samples.x >> offset) & SPT_VARIABLE_RATE_MASK);
				vrSample = MinVariableRate(vrSample, (samples.y >> offset) & SPT_VARIABLE_RATE_MASK);
				vrSample = MinVariableRate(vrSample, (samples.z >> offset) & SPT_VARIABLE_RATE_MASK);
				vrSample = MinVariableRate(vrSample, (samples.w >> offset) & SPT_VARIABLE_RATE_MASK);

				variableRateMask |= vrSample << offset;
			}
		}
	}

	return variableRateMask;
}

float3 VariableRateMaskToDebugColor(in uint variableRateMask)
{
	float3 color = 0;

#if SPT_VARIABLE_RATE_MODE > SPT_VARIABLE_RATE_MODE_2X2
	if(variableRateMask == SPT_VARIABLE_RATE_4X4)
	{
		color = float3(0.1f, 0.1f, 0.1f);
	}
	else if(variableRateMask > SPT_VARIABLE_RATE_2X2)
	{
		color = float3(0.1f, 0.1f, 0.6f);
	}
	else 
#endif // SPT_VARIABLE_RATE_MODE > SPT_VARIABLE_RATE_MODE_2X2
	if(variableRateMask == SPT_VARIABLE_RATE_2X2)
	{
		color = float3(0, 0, 1);
	}
	else if(variableRateMask > SPT_VARIABLE_RATE_1X1)
	{
		color = float3(0, 1, 0);
	}
	else
	{
		color = float3(1, 0, 0);
	}

	return color;
}


uint PackVRBlockInfo(in uint2 localOffset, in uint vrMask)
{
	const uint packedLocalX = localOffset.x & 3;
	const uint packedLocalY = (localOffset.y & 3) << 2;
	const uint packedVRMask = vrMask << 4;
	return packedLocalX | packedLocalY | packedVRMask;
}


void UnpackVRBlockInfo(in uint packedVRBlockInfo, out uint2 localOffset, out uint vrMask)
{
	localOffset.x = packedVRBlockInfo & 3;
	localOffset.y = (packedVRBlockInfo >> 2) & 3;
	vrMask        = packedVRBlockInfo >> 4;
}

#endif // VARIABLE_RATE_HLSLI