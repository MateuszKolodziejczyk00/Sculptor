#ifndef VARIABLE_RATE_HLSLI
#define VARIABLE_RATE_HLSLI

#define SPT_VARIABLE_RATE_MODE_2X2 0u
#define SPT_VARIABLE_RATE_MODE_4X4 1u

#ifndef SPT_VARIABLE_RATE_MODE
#define SPT_VARIABLE_RATE_MODE SPT_VARIABLE_RATE_MODE_4X4
#endif // SPT_VARIABLE_RATE_MODE

#define SPT_VARIABLE_RATE_2X (1u << 0)
#define SPT_VARIABLE_RATE_2Y (1u << 1)

#if SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
	#define SPT_VARIABLE_RATE_4X (1u << 2)
	#define SPT_VARIABLE_RATE_4Y (1u << 3)
#endif // SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4

#if SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_2X2
	#define SPT_VARIABLE_RATE_BITS 2u
#elif SPT_VARIABLE_RATE_MODE == SPT_VARIABLE_RATE_MODE_4X4
	#define SPT_VARIABLE_RATE_BITS 4u
#endif // SPT_VARIABLE_RATE_MODE
#define SPT_VARIABLE_RATE_MASK ((1u << SPT_VARIABLE_RATE_BITS) - 1)

#define SPT_VARIABLE_RATE_1X1	(0u)
#define SPT_VARIABLE_RATE_2X2	(SPT_VARIABLE_RATE_2X | SPT_VARIABLE_RATE_2Y)

#if SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
	#define SPT_VARIABLE_RATE_4X4	(SPT_VARIABLE_RATE_4X | SPT_VARIABLE_RATE_4Y)
#endif // SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4

#define SPT_VARIABLE_RATE_2X2_MASK	SPT_VARIABLE_RATE_2X2
#if SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
	#define SPT_VARIABLE_RATE_4X4_MASK	SPT_VARIABLE_RATE_4X4
#endif // SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4

#if SPT_VARIABLE_RATE_MODE >= SPT_VARIABLE_RATE_MODE_4X4
#define SPT_INVALID_VARIABLE_RATE 0xF
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


uint MaxVariableRate(in uint a, in uint b)
{
	const uint ax = a & SPT_VARIABLE_RATE_X_MASK;
	const uint ay = a & SPT_VARIABLE_RATE_Y_MASK;

	const uint bx = b & SPT_VARIABLE_RATE_X_MASK;
	const uint by = b & SPT_VARIABLE_RATE_Y_MASK;

	return max(ax, bx) | max(ay, by);
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


uint CreateCompressedVariableRateData(in uint variableRateMask)
{
	uint compressedData = 0;
	compressedData |= variableRateMask << (SPT_VARIABLE_RATE_BITS * 0);
	compressedData |= variableRateMask << (SPT_VARIABLE_RATE_BITS * 1);
	compressedData |= variableRateMask << (SPT_VARIABLE_RATE_BITS * 2);
	compressedData |= variableRateMask << (SPT_VARIABLE_RATE_BITS * 3);

	return compressedData;
}


uint LoadVariableRate(in Texture2D<uint> vrTexture, in uint2 pixel)
{
	const uint variableRateData = vrTexture.Load(uint3(pixel, 0));

	return DecompressVariableRateFromTexture(variableRateData);
}


#ifndef USE_CONSERVATIVE_REPROJECTION
#define USE_CONSERVATIVE_REPROJECTION 0
#endif // USE_CONSERVATIVE_REPROJECTION


struct VariableRateReprojectionResult
{
	uint variableRateMask;
	bool isSuccessful;
};


VariableRateReprojectionResult ReprojectVariableRate(in Texture2D<uint> vrTexture, in float2 uv, in uint2 variableRateRes, in float2 motion)
{
	const float2 reprojectedUV = uv - motion;

	VariableRateReprojectionResult result = { SPT_VARIABLE_RATE_1X1, false };

	if(all(reprojectedUV >= 0.f) && all(reprojectedUV <= 1.f))
	{
		uint variableRateMask = 0;

#if USE_CONSERVATIVE_REPROJECTION
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
#else
		variableRateMask = vrTexture.Load(uint3(reprojectedUV * variableRateRes, 0));
#endif // USE_CONSERVATIVE_REPROJECTION

		result.variableRateMask = variableRateMask;
		result.isSuccessful     = true;
	}

	return result;
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
		color = float3(0.f, 0.f, 1.f);
	}
	else 
#endif // SPT_VARIABLE_RATE_MODE > SPT_VARIABLE_RATE_MODE_2X2
	if(variableRateMask == SPT_VARIABLE_RATE_2X2)
	{
		color = float3(0, 1, 0);
	}
	else if(variableRateMask > SPT_VARIABLE_RATE_1X1)
	{
		color = float3(1, 1, 0);
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


uint2 GetVariableBlockCoords(in Texture2D<uint> variableRateBlocksTexture, in uint2 pixel)
{
	const uint vrBlockInfo = variableRateBlocksTexture.Load(uint3(pixel, 0)).x;

	uint2 localTraceOffset = 0;
	uint vrMask = 0;
	UnpackVRBlockInfo(vrBlockInfo, OUT localTraceOffset, OUT vrMask);
	const uint2 blockSize = GetVariableRateTileSize(vrMask);
	const uint2 blockCoords = uint2(pixel.x & ~(blockSize.x - 1), pixel.y & ~(blockSize.y - 1));

	return blockCoords;
}


void GetVariableRateInfo(in RWTexture2D<uint> variableRateBlocksTexture, in uint2 pixel, out uint2 traceCoords, out uint variableRateMask)
{
	const uint vrBlockInfo = variableRateBlocksTexture[pixel];

	uint2 localTraceOffset = 0;
	UnpackVRBlockInfo(vrBlockInfo, OUT localTraceOffset, OUT variableRateMask);
	const uint2 blockSize = GetVariableRateTileSize(variableRateMask);
	const uint2 blockCoords = uint2(pixel.x & ~(blockSize.x - 1), pixel.y & ~(blockSize.y - 1));

	traceCoords = blockCoords + localTraceOffset;
}


uint2 GetVariableTraceCoords(in uint vrBlockInfo, in uint2 pixel)
{
	uint2 localTraceOffset = 0;
	uint vrMask = 0;
	UnpackVRBlockInfo(vrBlockInfo, OUT localTraceOffset, OUT vrMask);
	const uint2 blockSize = GetVariableRateTileSize(vrMask);
	const uint2 blockCoords = uint2(pixel.x & ~(blockSize.x - 1), pixel.y & ~(blockSize.y - 1));

	return blockCoords + localTraceOffset;
}


uint2 GetVariableTraceCoords(in Texture2D<uint> variableRateBlocksTexture, in uint2 pixel)
{
	const uint vrBlockInfo = variableRateBlocksTexture.Load(uint3(pixel, 0)).x;
	return GetVariableTraceCoords(vrBlockInfo, pixel);
}


uint2 GetVariableTraceCoords(in RWTexture2D<uint> variableRateBlocksTexture, in uint2 pixel)
{
	const uint vrBlockInfo = variableRateBlocksTexture[pixel];
	return GetVariableTraceCoords(vrBlockInfo, pixel);
}

#endif // VARIABLE_RATE_HLSLI