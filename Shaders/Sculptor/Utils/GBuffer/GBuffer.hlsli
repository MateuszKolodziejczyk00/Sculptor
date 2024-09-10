#ifndef GBUFFER_HLSLI
#define GBUFFER_HLSLI

#include "Utils/Quaternion.hlsli"


#define GBUFFER_FLAG_TANGENT_FRAME_INVERSE_HANDEDNESS 0x1


// 0 - 4 bytes [baseColor - 24bits] [metallic - 8bits]
// 1 - 4 bytes [tangent frame - 32bits]
// 2 - 2 bytes [roughness - 16 its]
// 3 - 4 bytes [emissive - 32bits]
// 4 - 1 byte  [flags - 8bits] - 0: tangent frame handedness, other bits are reserved

struct GBufferOutput
{
	float4 gBuffer0 : SV_TARGET0;
	float4 gBuffer1 : SV_TARGET1;
	float  gBuffer2 : SV_TARGET2;
	float3 gBuffer3 : SV_TARGET3;
	uint   gBuffer4 : SV_TARGET4;
};


struct GBufferInput
{
	Texture2D<float4> gBuffer0;
	Texture2D<float4> gBuffer1;
	Texture2D<float>  gBuffer2;
	Texture2D<float3> gBuffer3;
	Texture2D<uint>   gBuffer4;
};


struct GBufferData
{
	float3 baseColor;
	float  metallic;
	float3 normal;
	float3 tangent;
	float3 bitangent;
	float  roughness;
	float3 emissive;
};


GBufferData DecodeGBuffer(in GBufferInput input, in uint3 pixel)
{
	GBufferData data;

	const uint flags = input.gBuffer4.Load(pixel);

	float tangentFrameHandedness = flags & GBUFFER_FLAG_TANGENT_FRAME_INVERSE_HANDEDNESS ? -1.0f : 1.0f;
	const Quaternion tangentFrame = UnpackQuaternion(input.gBuffer1.Load(pixel));
	float3x3 tangentFrameMatrix = QuatTo3x3(tangentFrame);
	tangentFrameMatrix[1] *= tangentFrameHandedness;

	const float4 baseColorMetallic = input.gBuffer0.Load(pixel);

	data.baseColor = baseColorMetallic.rgb;
	data.metallic  = baseColorMetallic.a;
	data.normal    = tangentFrameMatrix[2];
	data.tangent   = tangentFrameMatrix[0];
	data.bitangent = tangentFrameMatrix[1];
	data.roughness = input.gBuffer2.Load(pixel);
	data.emissive  = input.gBuffer3.Load(pixel);

	return data;
}


float3 DecodeGBufferNormal(in float4 tangentFrame)
{
	return QuatTo3x3(UnpackQuaternion(tangentFrame))[2];
}


GBufferOutput EncodeGBuffer(in GBufferData data)
{
	GBufferOutput output;

	uint flags = 0;

	float3 bitangent = data.bitangent;
	const bool invertBitangent = dot(bitangent, cross(data.normal, data.tangent)) < 0.0f;
	const float handedness = invertBitangent ? -1.0f : 1.0f;
	if(invertBitangent)
	{
		flags |= GBUFFER_FLAG_TANGENT_FRAME_INVERSE_HANDEDNESS;
	}
	bitangent *= handedness;

	const Quaternion tangentFrame = QuatFrom3x3(float3x3(data.tangent, bitangent, data.normal));
	const float4 packedTangentFrame = PackQuaternion(tangentFrame);

	output.gBuffer0 = float4(data.baseColor, data.metallic);
	output.gBuffer1 = packedTangentFrame;
	output.gBuffer2 = data.roughness;
	output.gBuffer3 = data.emissive;
	output.gBuffer4 = flags;

	return output;
}


#endif // GBUFFER_HLSLI