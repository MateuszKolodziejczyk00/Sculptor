#include "SculptorShader.hlsli"

[[descriptor_set(DownsampleGeometryTexturesDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"
#include "Shading/Shading.hlsli"
#include "Utils/Packing.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


static const float depthDiffThreshold = 0.04f;


int SelectSampleForDownsampledTexture(in bool useMinDepth, in float4 samplesDepth)
{
	if(useMinDepth)
	{
		float minDepth = 999999.f;
		int minDepthIdx = -1;
		for(int sampleIdx = 0; sampleIdx < 4; ++sampleIdx)
		{
			if(samplesDepth[sampleIdx] < minDepth)
			{
				minDepth = samplesDepth[sampleIdx];
				minDepthIdx = sampleIdx;
			}
		}

		return minDepthIdx;
	}
	else
	{
		float maxDepth = -999999.f;
		int maxDepthIdx = -1;
		for(int sampleIdx = 0; sampleIdx < 4; ++sampleIdx)
		{
			if(samplesDepth[sampleIdx] > maxDepth)
			{
				maxDepth = samplesDepth[sampleIdx];
				maxDepthIdx = sampleIdx;
			}
		}

		return maxDepthIdx;
	}
}


[numthreads(8, 8, 1)]
void DownsampleGeometryTexturesCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;

	if(all(pixel < u_constants.outputRes))
	{
		const uint2 inputBasePixel = pixel * 2;

		const float2 inputBaseUV = (float2(inputBasePixel) + 0.5f) * u_constants.inputPixelSize;

		const uint2 samplesOffset[4] = { uint2(0, 1), uint2(1, 1), uint2(1, 0), uint2(0, 0) };

		const float4 depths = u_depthTexture.Gather(u_nearestSampler, inputBaseUV, 0);

		const float2 outputUV = (float2(pixel) + 0.5f) * u_constants.outputPixelSize;

		const Ray outputCameraRay = CreateViewRayWS(u_sceneView, outputUV);

		float4 intersectionDepths = depths;
		const float minDepth = min(min(min(depths.x, depths.y), depths.z), depths.w);

		for(int sampleIdx = 0; sampleIdx < 4; ++sampleIdx)
		{
			if(!IsNearlyZero(depths[sampleIdx]))
			{
				const uint3 inputPixel = uint3(min(inputBasePixel + samplesOffset[sampleIdx], u_constants.inputRes - 1), 0);
				const float2 inputUV = (float2(inputPixel.xy) + 0.5f) * u_constants.inputPixelSize;

				const float3 locationWS = NDCToWorldSpace(float3(inputUV * 2.f - 1.f, depths[sampleIdx]), u_sceneView);

				const float4 tangentFrame = u_tangentFrameTexture.Load(inputPixel);
				const float3 normal = DecodeGBufferNormal(tangentFrame);

				const Plane samplePlane = Plane::Create(normal, locationWS);

				const IntersectionResult intersection = outputCameraRay.IntersectPlane(samplePlane);
				if(intersection.IsValid())
				{
					const float3 intersectionWS = outputCameraRay.GetIntersectionLocation(intersection);
					const float3 intersectionNDC = WorldSpaceToNDC(intersectionWS, u_sceneView);

					if(intersectionNDC.z > minDepth)
					{
						intersectionDepths[sampleIdx] = intersectionNDC.z;
					}
				}
			}
		}

		const bool useMinDepth = ((pixel.x + pixel.y) & 1) == 1;
		const int mostImportantSampleIdx = SelectSampleForDownsampledTexture(useMinDepth, intersectionDepths);

		const uint3 inputPixel = uint3(min(inputBasePixel + samplesOffset[mostImportantSampleIdx], u_constants.inputRes - 1), 0);
		const float2 inputUV = (float2(inputPixel.xy) + 0.5f) * u_constants.inputPixelSize;

		const float4 tangentFrame = u_tangentFrameTexture.Load(inputPixel);
		const float3 normal = DecodeGBufferNormal(tangentFrame);

		const float depth = intersectionDepths[mostImportantSampleIdx];

		const float2 motion = u_motionTexture.Load(inputPixel);

		u_depthTextureHalfRes[pixel]   = depth;
		u_motionTextureHalfRes[pixel]  = motion;
		u_normalsTextureHalfRes[pixel] = OctahedronEncodeNormal(normal);

		if(u_constants.downsampleSpecularColor)
		{
			const float4 baseColorMetalic = u_baseColorMetallicTexture.Load(inputPixel);

			float3 diffuseColor = 0.f;
			float3 specularColor = 0.f;
			ComputeSurfaceColor(baseColorMetalic.rgb, baseColorMetalic.w, OUT diffuseColor, OUT specularColor);
			u_specularColorTextureHalfRes[pixel] = specularColor;
		}

		if(u_constants.downsampleRoughness)
		{
			const float roughness = u_roughnessTexture.Load(inputPixel);
			u_roughnessTextureHalfRes[pixel] = roughness;
		}
	}
}
