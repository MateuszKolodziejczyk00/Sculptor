#ifndef PACKING_HLSLI
#define PACKING_HLSLI

#include "Sculptor/Utils/ColorSpaces.hlsli"

// RGB to LogLuv conversion
// Based on: https://github.com/NVIDIAGameWorks/MathLib/blob/main/STL.hlsli

// Encode an RGB color into a 32-bit LogLuv HDR format. The supported luminance range is roughly 10^-6..10^6 in 0.17% steps.
// The log-luminance is encoded with 14 bits and chroma with 9 bits each. This was empirically more accurate than using 8 bit chroma.
// Black (all zeros) is handled exactly
uint EncodeRGBToLogLuv(in float3 x)
{
	// Convert RGB to XYZ
	const float3 XYZ = RGBToXYZ(x);

	// Encode log2( Y ) over the range [ -20, 20 ) in 14 bits ( no sign bit ).
	// TODO: Fast path that uses the bits from the fp32 representation directly
	const float logY = 409.6f * (log2(XYZ.y) + 20.f); // -inf if Y == 0
	const uint Le = uint(clamp(logY, 0.f, 16383.f));

	// Early out if zero luminance to avoid NaN in chroma computation.
	// Note Le == 0 if Y < 9.55e-7. We'll decode that as exactly zero
	if(Le == 0)
	{
		return 0;
	}

	// Compute chroma (u,v) values by:
	//  x = X / ( X + Y + Z )
	//  y = Y / ( X + Y + Z )
	//  u = 4x / ( -2x + 12y + 3 )
	//  v = 9y / ( -2x + 12y + 3 )
	// These expressions can be refactored to avoid a division by:
	//  u = 4X / ( -2X + 12Y + 3(X + Y + Z) )
	//  v = 9Y / ( -2X + 12Y + 3(X + Y + Z) )
	const float invDenom = 1.f / (-2.0 * XYZ.x + 12.f * XYZ.y + 3.f * (XYZ.x + XYZ.y + XYZ.z));
	const float2 uv = float2(4.f, 9.f) * XYZ.xy * invDenom;

	// Encode chroma (u,v) in 9 bits each.
	// The gamut of perceivable uv values is roughly [0,0.62], so scale by 820 to get 9-bit values
	const uint2 uve = uint2(clamp(820.0 * uv, 0.0, 511.0));

	return (Le << 18) | (uve.x << 9) | uve.y;
}

// Decode an RGB color stored in a 32-bit LogLuv HDR format
float3 DecodeRGBFromLogLuv(uint x)
{
	// Decode luminance Y from encoded log-luminance
	const uint Le = x >> 18;
	if(Le == 0)
	{
		return 0;
	}

	const float logY = (float(Le) + 0.5f) / 409.6f - 20.0f;
	const float Y = exp2(logY);

	// Decode normalized chromaticity xy from encoded chroma (u,v)
	//  x = 9u / (6u - 16v + 12)
	//  y = 4v / (6u - 16v + 12)
	const uint2 uve = uint2(x >> 9, x) & 0x1ff;
	const float2 uv = (float2(uve) + 0.5f) / 820.f;

	const float invDenom = 1.f / (6.f * uv.x - 16.f * uv.y + 12.f);
	const float2 xy = float2(9.f, 4.f) * uv * invDenom;

	// Convert chromaticity to XYZ and back to RGB.
	//  X = Y / y * x
	//  Z = Y / y * (1 - x - y)
	const float s = Y / xy.y;
	const float3 XYZ = float3(s * xy.x, Y, s * (1.f - xy.x - xy.y));

	// Convert back to RGB and clamp to avoid out-of-gamut colors
	const  float3 color = max(XYZToRGB(XYZ), 0.f);

	return color;
}

// Octahedral normal encoding

float2 OctahedronEncodeNormal(in float3 direction)
{
	const float l1norm = dot(abs(direction), 1.f);
	float2 uv = direction.xy * (1.f / l1norm);
	if (direction.z < 0.f)
	{
		uv = (1.f - abs(uv.yx)) * SignNotZero(direction.xy);
	}
	return uv * 0.5f + 0.5f;
}

 
float3 OctahedronDecodeNormal(in float2 coords)
{
	coords = coords * 2.f - 1.f;

	float3 direction = float3(coords.x, coords.y, 1.f - abs(coords.x) - abs(coords.y));
	if (direction.z < 0.f)
	{
		direction.xy = (1.f - abs(direction.yx)) * SignNotZero(direction.xy);
	}
	return normalize(direction);
}

float3 OctahedronDecodeHemisphereNormal(in float2 coords)
{
	coords = coords * 2.f - 1.f;

    const float2x2 coordsTransform = float2x2(
        0.5f, -0.5f,
        0.5f, 0.5f
    );

    coords = mul(coordsTransform, coords);

	float3 direction = float3(coords.x, coords.y, 1.f - abs(coords.x) - abs(coords.y));

	return normalize(direction);
}


float2 OctahedronEncodeHemisphereNormal(in float3 direction)
{
    const float l1norm = dot(abs(direction), 1.f);
	float2 coords = direction.xy * (1.f / l1norm);

    const float2x2 coordsTransform = float2x2(
        1.f,  1.f,
        -1.f, 1.f
    );

    coords = mul(coordsTransform, coords);

    return coords * 0.5f + 0.5f;
}

// rgba8 packing

uint PackFloat4x8(float4 value)
{
	const uint4 asUints = value * 255.f;
	return (asUints.r << 24) | (asUints.g << 16) | (asUints.b << 8) | asUints.a;
}


float4 UnpackFloat4x8(uint value)
{
	const uint4 asUints = uint4((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
	return asUints / 255.f;
}

uint PackHalf2x16Norm(in float2 value)
{
	const uint2 asUints = value * 65535.f;
	return (asUints.x << 16) | asUints.y;
}

float2 UnpackHalf2x16Norm(uint value)
{
	const uint2 asUints = uint2((value >> 16) & 0xFFFF, value & 0xFFFF);
	return asUints / 65535.f;
}

uint16_t PackFloatNorm(in float value)
{
	return uint16_t(clamp(value, 0.f, 1.f) * 65535.f);
}

float UnpackFloatNorm(in uint16_t value)
{
	return float(value) / 65535.f;
}

uint PackHalf(in half value)
{
	return asuint16(value);
}

half UnpackHalf(in uint16_t value)
{
	const uint packed = value;
	return (half)f16tof32(packed);
}

#endif // PACKING_HLSLI