#ifndef GEOMETRY_COMMON_H
#define GEOMETRY_COMMON_H

uint PackVisibilityInfo(in uint visibleMeshletIdx, in uint triangleIdx)
{
	SPT_CHECK_MSG(triangleIdx < (1 << 7), L"Invalid triangle index - {}", triangleIdx);
	return (visibleMeshletIdx << 7) | triangleIdx;
}


bool UnpackVisibilityInfo(in uint packedInfo, out uint visibleMeshletIdx, out uint triangleIdx)
{
	visibleMeshletIdx = packedInfo >> 7;
	triangleIdx = packedInfo & 0x7F;
	return (visibleMeshletIdx != (~0u >> 7));
}


float MaterialBatchIdxToMaterialDepth(in uint materialBatchIdx)
{
	return materialBatchIdx / float(0xFFFF);
}


float MaterialDepthToMaterialBatchIdx(in float materialDepth)
{
	return materialDepth * float(0xFFFF);
}


struct Barycentrics
{
	float3 uvw;
	float3 duvw_dx;
	float3 duvw_dy;
};


Barycentrics ComputeTriangleBarycentrics(in float2 p, in float4 cs0, in float4 cs1, in float4 cs2, in float2 rcpViewportSize)
{
	const float3 rcpW = rcp(float3(cs0.w, cs1.w, cs2.w));

	const float2 a = cs0.xy * rcpW[0];
	const float2 b = cs1.xy * rcpW[1];
	const float2 c = cs2.xy * rcpW[2];

	// Edge functions:
	// Ecb = (c.y - b.y) * (p.x - b.x) + (b.x - c.x) * (p.y - b.y)
	// Eca = (a.y - c.y) * (p.x - c.x) + (c.x - a.x) * (p.y - c.y)
	// Eab = (b.y - a.y) * (p.x - a.x) + (a.x - b.x) * (p.y - a.y)

	// Derivatives of edge functions:
	// dEcb_dpx = (c.y - b.y) * (d/dpx) * (p.x - b.x) + (b.x - c.x) * (d/dpx) * (p.y - b.y) =
	// = (c.y - b.y) - (1 - 0) + (b.x - c.x) * (0 - 0) = c.y - b.y
	// dEcb_dpy = (c.y - b.y) * (d/dpy) * (p.x - b.x) + (b.x - c.x) * (d/dpy) * (p.y - b.y) =
	// = (c.y - b.y) * (0 - 0) + (b.x - c.x) - (1 - 0) = b.x - c.x
	// dEca_dpx = a.y - c.y
	// dEca_dpy = c.x - a.x
	// dEab_dpx = b.y - a.y
	// dEab_dpy = a.x - b.x

	const float3 BCA_X = float3(b.x, c.x, a.x);
	const float3 BCA_Y = float3(b.y, c.y, a.y);
	const float3 CAB_X = float3(c.x, a.x, b.x);
	const float3 CAB_Y = float3(c.y, a.y, b.y);

	const float3 dE_dpx = CAB_Y - BCA_Y;
	const float3 dE_dpy = BCA_X - CAB_X;

	const float3 E = dE_dpx * (p.x - BCA_X) + dE_dpy * (p.y - BCA_Y);

	// Compute the unnormalized barycentrics
	const float3 ub = E * rcpW;

	const float ew = dot(E, rcpW);
	const float rcpEW = rcp(ew);

	const float3 uvw = ub * rcpEW;

	// uvw = ub / ew
	// uvw' = (ub / ew)' = (ew * ub' - ub * ew') / ew^2

	const float3 dub_dpx = dE_dpx * rcpW;
	const float3 dub_dpy = dE_dpy * rcpW;

	const float3 dw_dpx = dot(dE_dpx, rcpW);
	const float3 dw_dpy = dot(dE_dpy, rcpW);

	const float3 duvw_dpx = (ew * dub_dpx - ub * dw_dpx) * Pow2(rcpEW);
	const float3 duvw_dpy = (ew * dub_dpy - ub * dw_dpy) * Pow2(rcpEW);

	Barycentrics result;
	result.uvw = uvw;
	result.duvw_dx = duvw_dpx * (2.f * rcpViewportSize.x);
	result.duvw_dy = duvw_dpy * (-2.f * rcpViewportSize.y);

	return result;
}


TextureCoord InterpolateTextureCoord(in float2 uv0, in float2 uv1, in float2 uv2, in Barycentrics barycentrics)
{
	const float2 uv10 = uv1 - uv0;
	const float2 uv20 = uv2 - uv0;
	
	const float2 uv = uv10 * barycentrics.uvw[1] + uv20 * barycentrics.uvw[2] + uv0;

	const float2 duv_dx = uv10 * barycentrics.duvw_dx[1] + uv20 * barycentrics.duvw_dx[2];
	const float2 duv_dy = uv10 * barycentrics.duvw_dy[1] + uv20 * barycentrics.duvw_dy[2];

	TextureCoord result;
	result.uv     = uv;
	result.duv_dx = duv_dx;
	result.duv_dy = duv_dy;

	return result;
}


template<typename TDataType>
TDataType InterpolateAttribute(in TDataType a0, in TDataType a1, in TDataType a2, in Barycentrics barycentrics)
{
	return a0 * barycentrics.uvw[0] + a1 * barycentrics.uvw[1] + a2 * barycentrics.uvw[2];
}


#ifdef DS_GeometryDS
uint3 LoadTriangleVertexIndices(uint meshletPrimitivesOffset, uint meshletTriangleIdx)
{
	const uint triangleStride = 3;
	const uint primitiveOffset = meshletPrimitivesOffset + meshletTriangleIdx * triangleStride;

	// Load multiple of 4
	const uint primitiveOffsetToLoad = primitiveOffset & 0xfffffffc;

	// Load uint2 to be sure that we will have all 3 indices
	const uint2 meshletPrimitivesIndices = u_geometryData.Load<uint2>(primitiveOffsetToLoad);

	uint primitiveIndicesByteOffset = primitiveOffset - primitiveOffsetToLoad;

	uint3 traingleIndices;

	for (int idx = 0; idx < 3; ++idx, ++primitiveIndicesByteOffset)
	{
		uint indices4;
		uint offset;
		
		if(primitiveIndicesByteOffset >= 4)
		{
			indices4 = meshletPrimitivesIndices[1];
			offset = primitiveIndicesByteOffset - 4;
		}
		else
		{
			indices4 = meshletPrimitivesIndices[0];
			offset = primitiveIndicesByteOffset;
		}

		traingleIndices[idx] = (indices4 >> (offset * 8)) & 0x000000ff;
	}

	return traingleIndices;
}
#endif // DS_GeometryDS


#endif // GEOMETRY_COMMON_H