#include "SculptorShader.hlsli"

[[shader_params(DownsampleGeometryTexturesConstants, PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES)]]
[[shader_params(GPURenderView, VIEW)]]

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

	if(all(pixel < PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->outputRes))
	{
		const uint2 inputBasePixel = pixel * 2;

		const float2 inputBaseUV = (float2(inputBasePixel) + 0.5f) * PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->inputPixelSize;

		const uint2 samplesOffset[4] = { uint2(0, 1), uint2(1, 1), uint2(1, 0), uint2(0, 0) };

		const float4 depths = PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->depthTexture.Gather(BindlessSamplers::NearestClampEdge(), inputBaseUV);

		const float2 outputUV = (float2(pixel) + 0.5f) * PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->outputPixelSize;

		const Ray outputCameraRay = CreateViewRayWS(VIEW->sceneView, outputUV);

		float4 intersectionDepths = depths;
		const float minDepth = min(min(min(depths.x, depths.y), depths.z), depths.w);

		for(int sampleIdx = 0; sampleIdx < 4; ++sampleIdx)
		{
			if(!IsNearlyZero(depths[sampleIdx]))
			{
				const uint3 inputPixel = uint3(min(inputBasePixel + samplesOffset[sampleIdx], PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->inputRes - 1), 0);
				const float2 inputUV = (float2(inputPixel.xy) + 0.5f) * PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->inputPixelSize;

				const float3 locationWS = NDCToWorldSpace(float3(inputUV * 2.f - 1.f, depths[sampleIdx]), VIEW->sceneView);

				const float4 tangentFrame = PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->tangentFrameTexture.Load(inputPixel);
				const float3 normal = DecodeGBufferNormal(tangentFrame);

				const Plane samplePlane = Plane::Create(normal, locationWS);

				const IntersectionResult intersection = outputCameraRay.IntersectPlane(samplePlane);
				if(intersection.IsValid())
				{
					const float3 intersectionWS = outputCameraRay.GetIntersectionLocation(intersection);
					const float3 intersectionNDC = WorldSpaceToNDC(intersectionWS, VIEW->sceneView);

					if(intersectionNDC.z > minDepth)
					{
						intersectionDepths[sampleIdx] = intersectionNDC.z;
					}
				}
			}
		}

		const bool useMinDepth = ((pixel.x + pixel.y) & 1) == 1;
		const int mostImportantSampleIdx = SelectSampleForDownsampledTexture(useMinDepth, intersectionDepths);

		const uint3 inputPixel = uint3(min(inputBasePixel + samplesOffset[mostImportantSampleIdx], PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->inputRes - 1), 0);
		const float2 inputUV = (float2(inputPixel.xy) + 0.5f) * PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->inputPixelSize;

		const float4 tangentFrame = PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->tangentFrameTexture.Load(inputPixel);
		const float3 normal = DecodeGBufferNormal(tangentFrame);

		const float depth = intersectionDepths[mostImportantSampleIdx];

		const float2 motion = PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->motionTexture.Load(inputPixel);

		PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->depthTextureHalfRes[pixel]   = depth;
		PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->motionTextureHalfRes[pixel]  = motion;
		PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->normalsTextureHalfRes[pixel] = OctahedronEncodeNormal(normal);

		if(PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->downsampleBaseColor)
		{
			const float4 baseColorMetalic = PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->baseColorMetallicTexture.Load(inputPixel);
			PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->baseColorTextureHalfRes[pixel] = baseColorMetalic;
		}

		if(PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->downsampleRoughness)
		{
			const float roughness = PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->roughnessTexture.Load(inputPixel);
			PARAMS_DOWNSAMPLE_GEOMETRY_TEXTURES->roughnessTextureHalfRes[pixel] = roughness;
		}
	}
}
