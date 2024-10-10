#include "SculptorShader.hlsli"

[[descriptor_set(ApplyInvalidationMaskDS, 0)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/MortonCode.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 localID  : SV_GroupThreadID;
};


#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8


[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void ApplyInvalidationMaskCS(CS_INPUT input)
{
	const uint2 coords = min(input.globalID.xy, u_resamplingConstants.resolution - 1u);

	const uint2 maskTileCoords = coords / uint2(8u, 4u);
	const uint2 tileLocalCoords = coords & uint2(7u, 3u);

	const uint invalidationMask = u_invalidationMask.Load(uint3(maskTileCoords, 0u)).x;

	const uint bitIndex = EncodeMorton2(tileLocalCoords);
	const bool isInvalidated = ((invalidationMask >> bitIndex) & 1u);

	const uint reservoirIdx = GetScreenReservoirIdx(coords, u_resamplingConstants.reservoirsResolution);

	const SRPackedReservoir packedReservoir = u_inOutReservoirsBuffer[reservoirIdx];

	const bool reservoirIsVolatile = HasPackedFlag(packedReservoir, SR_RESERVOIR_FLAGS_VOLATILE);

	if(reservoirIsVolatile)
	{
		const uint2 quadCoords = coords / 2u;
		const uint2 quadTextureCoords = quadCoords / uint2(8u, 4u);
		const uint quadBitOffset = dot(quadCoords & uint2(7u, 3u), uint2(1u, 8u));

		InterlockedOr(u_rwQuadVolatilityMask[quadTextureCoords], 1u << quadBitOffset);
	}

	if (isInvalidated)
	{
		const float resampledReservoirWeight = Luminance(DecodeRGBFromLogLuv(u_inOutReservoirsBuffer[reservoirIdx].packedLuminance)) * u_inOutReservoirsBuffer[reservoirIdx].weight;

		SRPackedReservoir initialReservoir = u_initialReservoirsBuffer[reservoirIdx];

		uint packedProps = initialReservoir.MAndProps;
		packedProps = ModifyPackedReservoirM(packedProps, 1u);
		packedProps = ModifyPackedSpatialResamplingRangeID(packedProps, -6);

		initialReservoir.MAndProps = packedProps;

		u_inOutReservoirsBuffer[reservoirIdx] = initialReservoir;
			
		const float initialReservoirWeight = Luminance(DecodeRGBFromLogLuv(initialReservoir.packedLuminance)) * initialReservoir.weight;

		const float weightRatio = resampledReservoirWeight / initialReservoirWeight;
		const uint unreliablePixelsNum = WaveActiveCountBits(weightRatio > 3.f);

		const uint invalidatedReservoirsNum = WaveActiveCountBits(isInvalidated);
		const bool isTileUnreliable = invalidatedReservoirsNum >= 24u && unreliablePixelsNum > 7u;
	
		if (WaveIsFirstLane())
		{
			const uint2 tileTextureCoords = maskTileCoords / uint2(8u, 4u);
			const uint tileBitOffset      = dot(maskTileCoords, uint2(1u, 8u));
	
			if (isTileUnreliable)
			{
				InterlockedAnd(u_rwTileReliabilityMask[tileTextureCoords], ~(1u << tileBitOffset));
			}
		}
	}
}
