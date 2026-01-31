#ifndef UGB_HLSLI
#define UGB_HLSLI

[[shader_struct(SceneGeometryData)]]


struct UGBInterface : SceneGeometryData
{
	uint LoadVertexIndex(in uint ugbOffset, in uint idx)
	{
		const uint idxOffset = idx * 4;
		return ugb.geometryData.Load<uint>(ugbOffset + idxOffset);
	}

	float3 LoadLocation(in uint ugbOffset, in uint vertex)
	{
		const uint vertexOffset = vertex * 12;
		return ugb.geometryData.Load<float3>(ugbOffset + vertexOffset);
	}

	float3 LoadNormal(in uint ugbOffset, in uint vertex)
	{
		const uint vertexOffset = vertex * 4;
		const uint encodedNormal = ugb.geometryData.Load<uint>(ugbOffset + vertexOffset);
		const float2 octNormal = float2(
			(encodedNormal & 0xFFFF) / 65535.0f,
			((encodedNormal >> 16) & 0xFFFF) / 65535.0f
		);
		return OctahedronDecodeNormal(octNormal);
	}

	float4 LoadTangent(in uint ugbOffset, in uint vertex)
	{
		const uint vertexOffset = vertex * 4;
		const uint packedTangent = ugb.geometryData.Load<uint>(ugbOffset + vertexOffset);

		const float2 octTangent = float2(
			(packedTangent & 0x7FFF) / 32767.0f,
			((packedTangent >> 15) & 0x7FFF) / 32767.0f
		);

		const float3 tangent = OctahedronDecodeNormal(octTangent);
		const float handedness = ((packedTangent >> 30) & 1u) ? 1.0f : -1.0f;
		return float4(tangent, handedness);
	}

	float2 LoadNormalizedUV(in uint ugbOffset, in uint vertex)
	{
		const uint vertexOffset = vertex * 4;
		const uint packedUV = ugb.geometryData.Load<uint>(ugbOffset + vertexOffset);
		return float2(
			(packedUV & 0xFFFF) / 65535.0f,
			((packedUV >> 16) & 0xFFFF) / 65535.0f
		);
	}
};


#ifdef DS_RenderSceneDS

UGBInterface UGB()
{
	return UGBInterface(u_renderSceneConstants.geometry);
}

#endif // DS_RenderSceneDS

#endif // UGB_HLSLI
