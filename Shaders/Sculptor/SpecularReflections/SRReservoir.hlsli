#ifndef SRRESERVOIR_HLSLI
#define SRRESERVOIR_HLSLI

#include "Utils/Packing.hlsli"
#include "Utils/MortonCode.hlsli"

[[shader_struct(SRPackedReservoir)]]


#define SR_RESERVOIR_FLAGS_NONE       0
#define SR_RESERVOIR_FLAGS_MISS       (1 << 0)
#define SR_RESERVOIR_FLAGS_RECENT     (1 << 1) // Reservoir was traced recently and we can skip visibility check

// Flags that are inherited during resampling
#define SR_RESERVOIR_FLAGS_TRANSIENT  (SR_RESERVOIR_FLAGS_MISS | SR_RESERVOIR_FLAGS_RECENT)

#define SR_RESERVOIR_MAX_M   (255)
#define SR_RESERVOIR_MAX_AGE (255)

#define SR_RESERVOIR_DEFAULT_SPATIAL_RANGE_ID (8)


class SRReservoir
{
	static SRReservoir CreateEmpty()
	{
		SRReservoir reservoir;
		reservoir.hitLocation = ZERO_VECTOR;
		reservoir.hitNormal   = ZERO_VECTOR;
		reservoir.luminance   = ZERO_VECTOR;
		reservoir.M           = 0u;
		reservoir.weightSum   = 0.f;
		reservoir.flags       = 0u;
		reservoir.age         = 0u;
		reservoir.spatialResamplingRangeID = 0u;

		return reservoir;
	}

	static SRReservoir Create(in float3 hitLocation, in float3 hitNormal, in float3 luminance, in float pdf)
	{
		SRReservoir reservoir;
		reservoir.hitLocation = hitLocation;
		reservoir.hitNormal   = hitNormal;
		reservoir.luminance   = luminance;
		reservoir.M           = 1u;
		reservoir.weightSum   = !IsNearlyZero(pdf) ? rcp(pdf) : 0.f;
		reservoir.flags       = 0u;
		reservoir.age         = 0u;
		reservoir.spatialResamplingRangeID = 0u;

		return reservoir;
	}

	bool IsValid()
	{
		return M > 0;
	}

	bool Update(in SRReservoir other, in float randomValue, in float p_hatInOutputDomain)
	{
		const float w_i = other.M * other.weightSum * p_hatInOutputDomain;

		weightSum += w_i;
		M += other.M;

		const bool updateReservoir = (randomValue * weightSum <= w_i);

		if(updateReservoir)
		{
			hitLocation = other.hitLocation;
			hitNormal   = other.hitNormal;
			luminance   = other.luminance;
			age         = other.age;

			flags &= ~(SR_RESERVOIR_FLAGS_TRANSIENT);
			flags |= (other.flags & SR_RESERVOIR_FLAGS_TRANSIENT);
		}

		return updateReservoir;
	}

	// Default normalization function
	// uses 1/M as mis weight (which leads to bias)
	void Normalize(in float selectedPhat)
	{
		// Equation (7) from Restir GI paper
		// w = (1 / p_hat) * (1 / M) * sum(w_i) = sum(w_i) / (p_hat * M)
		const float denominator = selectedPhat * M;
		weightSum = denominator == 0.f ? 0.f : (weightSum / denominator);
	}

	void Normalize_BalancedHeuristic(in float selectedPhat, in float p_i, in float p_jSum)
	{
		// Normalization equation:
		// w = (1 / p_hat) * m_i(x) * sum(w_i)
		// We use balanced heuristic for m_i(x) which gives:
		// m_i(x) = p_i / (p_jSum)
		// w = (1 / p_hat) * (p_i / p_jSum) * sum(w_i) = sum(w_i) / (p_hat * p_jSum)
		const float denominator = p_jSum * selectedPhat;
		weightSum = denominator == 0.f ? 0.f : (weightSum * p_i / denominator);
	}

	void AddFlag(in uint flag)
	{
		flags |= uint16_t(flag);
	}

	bool HasFlag(in uint flag)
	{
		return (flags & uint16_t(flag)) != 0;
	}

	void RemoveFlag(in uint flag)
	{
		flags &= ~uint16_t(flag);
	}

	void OnSpatialResamplingFailed()
	{
		spatialResamplingRangeID = uint16_t(max(spatialResamplingRangeID, 1u) - 1u);
	}

	void OnSpatialResamplingSucceeded()
	{
		spatialResamplingRangeID = uint16_t(min(spatialResamplingRangeID + 1u, 15u));
	}

	bool CanCombine(in SRReservoir other)
	{
		return true;
	}

	float3     hitLocation;
	uint16_t   flags;
	uint16_t   age;
	float3     hitNormal;
	uint16_t   M;
	uint16_t   spatialResamplingRangeID; // idx to range (0 - min, 15 - max)
	float3     luminance;
	float      weightSum;
};


SRPackedReservoir PackReservoir(in SRReservoir reservoir)
{
	const uint m                        = min(reservoir.M,   SR_RESERVOIR_MAX_M);
	const uint age                      = min(reservoir.age, SR_RESERVOIR_MAX_AGE);
	const uint flags                    = reservoir.flags;
	const uint spatialResamplingRangeID = reservoir.spatialResamplingRangeID;

	SRPackedReservoir packedReservoir;
	packedReservoir.hitLocation     = reservoir.hitLocation;
	packedReservoir.packedLuminance = EncodeRGBToLogLuv(reservoir.luminance);
	packedReservoir.hitNormal       = OctahedronEncodeNormal(reservoir.hitNormal);
	packedReservoir.weight          = reservoir.weightSum;
	packedReservoir.MAndProps       = m | (flags << 8) | (age << 16) | (spatialResamplingRangeID << 24);

	return packedReservoir;
}


SRReservoir UnpackReservoir(in SRPackedReservoir packedReservoir)
{
	SRReservoir reservoir;
	reservoir.hitLocation              = packedReservoir.hitLocation;
	reservoir.luminance                = DecodeRGBFromLogLuv(packedReservoir.packedLuminance);
	reservoir.hitNormal                = OctahedronDecodeNormal(packedReservoir.hitNormal);
	reservoir.weightSum                = packedReservoir.weight;
	reservoir.M                        = uint16_t((packedReservoir.MAndProps) & 255u);
	reservoir.flags                    = uint16_t((packedReservoir.MAndProps >> 8u) & 255u);
	reservoir.age                      = uint16_t((packedReservoir.MAndProps >> 16u) & 255u);
	reservoir.spatialResamplingRangeID = uint16_t((packedReservoir.MAndProps >> 24u) & 15u);

	return reservoir;
}


uint ModifyPackedReservoirM(in uint MAndProps, in uint newM)
{
	return (MAndProps & 0xFFFFFF00u) | (newM & 0xFFu);
}


uint ModifyPackedSpatialResamplingRangeID(in uint MAndProps, in int delta)
{
	const uint prevRangeID = MAndProps >> 24u;
	const uint newRangeID = prevRangeID > delta ? min(prevRangeID + delta, 15u) : 0u;
	return (MAndProps & 0x00FFFFFFu) | (newRangeID << 24u);
}


uint GetScreenReservoirIdx(in uint2 coords, in uint2 resolution)
{
	const uint2 tileSize = uint2(64u, 64u);
	const uint2 tilesRes = resolution >> 6;
	const uint2 tileCoords = coords >> 6;

	const uint tileIdx = tileCoords.y * tilesRes.x + tileCoords.x;

	const uint2 coordsInTile = coords & 63u;
	const uint reservoirIdxInTile = EncodeMorton2(coordsInTile);

	return tileIdx * Pow2(64u) + reservoirIdxInTile;
}

#endif // SRRESERVOIR_HLSLI