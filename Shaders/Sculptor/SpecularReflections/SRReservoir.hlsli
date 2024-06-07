#ifndef SRRESERVOIR_HLSLI
#define SRRESERVOIR_HLSLI

#include "Utils/Packing.hlsli"

[[shader_struct(SRPackedReservoir)]]


#define SR_RESERVOIR_FLAGS_NONE       0
#define SR_RESERVOIR_FLAGS_MISS       1

#define SR_RESERVOIR_FLAGS_TRANSIENT  (SR_RESERVOIR_FLAGS_MISS)

#define SR_RESERVOIR_MAX_M   (255)
#define SR_RESERVOIR_MAX_AGE (255)


class SRReservoir
{
	static SRReservoir CreateEmpty()
	{
		SRReservoir reservoir;
		reservoir.hitLocation = ZERO_VECTOR;
		reservoir.hitNormal   = ZERO_VECTOR;
		reservoir.luminance   = ZERO_VECTOR;
		reservoir.M           = 0;
		reservoir.weightSum   = 0.f;
		reservoir.flags       = 0;
		reservoir.age         = 0;

		return reservoir;
	}

	static SRReservoir Create(in float3 hitLocation, in float3 hitNormal, in float3 luminance, in float pdf)
	{
		SRReservoir reservoir;
		reservoir.hitLocation = hitLocation;
		reservoir.hitNormal   = hitNormal;
		reservoir.luminance   = luminance;
		reservoir.M           = 1;
		reservoir.weightSum   = !IsNearlyZero(pdf) ? rcp(pdf) : 0.f;
		reservoir.flags       = 0;
		reservoir.age         = 0;

		return reservoir;
	}

	bool IsValid()
	{
		return M > 0;
	}

	bool Update(in SRReservoir other, in float randomValue, in float targetPdf)
	{
		const float risWeight = other.M * other.weightSum * targetPdf;

		weightSum += risWeight;
		M += other.M;

		const bool updateReservoir = (randomValue * weightSum <= risWeight);

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
	void Normalize(in float selectedSampleTargetPdf)
	{
		// Equation (7) from Restir GI paper
		// w = (1 / p_hat) * (1 / M) * sum(w_i)
		const float denominator = selectedSampleTargetPdf * M;
		weightSum = IsNearlyZero(denominator) ? 0.f : (weightSum * rcp(denominator));
	}

	void AddFlag(in uint flag)
	{
		flags |= flag;
	}

	bool HasFlag(in uint flag)
	{
		return (flags & flag) != 0;
	}

	bool CanCombine(in SRReservoir other)
	{
		return true;
	}

	float3 hitLocation;
	uint   flags;
	float3 hitNormal;
	uint   M;
	float3 luminance;
	float  weightSum;
	uint   age;
};


SRPackedReservoir PackReservoir(in SRReservoir reservoir)
{
	const int m   = min(reservoir.M,   SR_RESERVOIR_MAX_M);
	const int age = min(reservoir.age, SR_RESERVOIR_MAX_AGE);

	SRPackedReservoir packedReservoir;
	packedReservoir.hitLocation     = reservoir.hitLocation;
	packedReservoir.packedLuminance = EncodeRGBToLogLuv(reservoir.luminance);
	packedReservoir.hitNormal       = OctahedronEncodeNormal(reservoir.hitNormal);
	packedReservoir.weight          = reservoir.weightSum;
	packedReservoir.MAndProps       = m | (reservoir.flags << 8) | (age << 16);

	return packedReservoir;
}


SRReservoir UnpackReservoir(in SRPackedReservoir packedReservoir)
{
	SRReservoir reservoir;
	reservoir.hitLocation = packedReservoir.hitLocation;
	reservoir.luminance   = DecodeRGBFromLogLuv(packedReservoir.packedLuminance);
	reservoir.hitNormal   = OctahedronDecodeNormal(packedReservoir.hitNormal);
	reservoir.weightSum   = packedReservoir.weight;
	reservoir.M           = (packedReservoir.MAndProps) & 255;
	reservoir.flags       = (packedReservoir.MAndProps >> 8) & 255;
	reservoir.age         = (packedReservoir.MAndProps >> 16) & 255;

	return reservoir;
}


uint GetScreenReservoirIdx(in uint2 coords, in uint2 resolution)
{
	return coords.x + coords.y * resolution.x;
}

#endif // SRRESERVOIR_HLSLI