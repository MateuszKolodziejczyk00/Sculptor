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
		const uint vertexOffset = vertex * 12;
		return ugb.geometryData.Load<float3>(ugbOffset + vertexOffset);
	}

	float4 LoadTangent(in uint ugbOffset, in uint vertex)
	{
		const uint vertexOffset = vertex * 16;
		return ugb.geometryData.Load<float4>(ugbOffset + vertexOffset);
	}

	float2 LoadUV(in uint ugbOffset, in uint vertex)
	{
		const uint vertexOffset = vertex * 8;
		return ugb.geometryData.Load<float2>(ugbOffset + vertexOffset);
	}
};


#ifdef DS_RenderSceneDS

UGBInterface UGB()
{
	return UGBInterface(u_renderSceneConstants.geometry);
}

#endif // DS_RenderSceneDS

#endif // UGB_HLSLI