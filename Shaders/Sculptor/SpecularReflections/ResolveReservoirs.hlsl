#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(CloudscapeProbesParams, CLOUDSCAPE)]]
[[shader_params(ResolveReservoirsParams, CONSTS)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Shading/Shading.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Atmosphere/VolumetricClouds/Cloudscape.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ResolveReservoirsCS(CS_INPUT input)
{
	uint2 pixel = input.globalID.xy;
	
	if(all(pixel < CONSTS->resamplingConstants->resolution))
	{
		uint2 reservoirCoords = pixel;

		const uint reservoirIdx = GetScreenReservoirIdx(pixel, CONSTS->resamplingConstants->reservoirsResolution);
		SRReservoir reservoir = UnpackReservoir(CONSTS->reservoirsBuffer[reservoirIdx]);

		const uint2 traceCoords = GetVariableTraceCoords(CONSTS->variableRateBlocksTexture, pixel);

		const bool isTracingCoord = all(pixel == traceCoords);

		const bool canUseReservoir = reservoir.IsValid() && reservoir.HasValidResult();

		if(!canUseReservoir && !isTracingCoord)
		{
			reservoirCoords = traceCoords;
			const uint tracingReservoirIdx = GetScreenReservoirIdx(traceCoords, CONSTS->resamplingConstants->reservoirsResolution);
			reservoir = UnpackReservoir(CONSTS->reservoirsBuffer[tracingReservoirIdx]);
		}

		float3 specularLo = 0.f;
		float3 diffuseLo  = 0.f;
		float3 lightDir  = 0.f;
		float hitDistance = SPT_NAN;

		if(reservoir.IsValid() && reservoir.HasValidResult())
		{
			const float depth = CONSTS->depthTexture.Load(uint3(reservoirCoords, 0));
			const float2 uv = (reservoirCoords + 0.5f) * CONSTS->resamplingConstants->pixelSize;
			const float3 ndc = float3(uv * 2.f - 1.f, depth);
	
			const float3 sampleLocation = NDCToWorldSpace(ndc, VIEW->sceneView);
	
			const float3 sampleNormal = OctahedronDecodeNormal(CONSTS->normalsTexture.Load(uint3(reservoirCoords, 0)));
	
			const float3 toView = normalize(VIEW->sceneView.viewLocation - sampleLocation);
	
			const float4 props = CONSTS->propsTexture.Load(uint3(reservoirCoords, 0));
			const float roughness = props.x;
			const float clearCoatWeight = props.y;
			const float clearCoatRoughness = props.z;

			const float4 baseColorMetallic = CONSTS->baseColorTexture.Load(uint3(reservoirCoords, 0));

			float3 f0;
			float3 diffuseColor;
			ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT f0);

			hitDistance = length(reservoir.hitLocation - sampleLocation);
			lightDir = (reservoir.hitLocation - sampleLocation) / hitDistance;

			const float NdotL = saturate(dot(sampleNormal, lightDir));
	
			float3 luminance = reservoir.luminance;

			if (!reservoir.HasFlag(SR_RESERVOIR_FLAGS_MISS))
			{
				luminance *= EvaluateHeightBasedTransmittanceForSegment(CONSTS->heightFogParams, sampleLocation, reservoir.hitLocation);
				luminance += LuminanceToExposedLuminance(SampleCloudscapeInScattering(sampleLocation, lightDir, hitDistance));
			}

			luminance *= reservoir.weightSum;

			ShadedSurface surface;
			surface.normal             = sampleNormal;
			surface.roughness          = roughness;
			surface.clearCoatWeight    = clearCoatWeight;
			surface.clearCoatRoughness = clearCoatRoughness;
			surface.f0                 = f0;
			surface.albedo             = diffuseColor;

			const BRDFLobes lobes = RTClosure::RTEvaluate(surface, toView, lightDir);
			specularLo = lobes.specular * luminance;
	
			const float NdotV = saturate(dot(sampleNormal, toView));
			const float2 integratedBRDF = GLOBALS->brdfIntegrationLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(NdotV, roughness), 0);
	
			if(any(isnan(specularLo)) || any(isinf(specularLo)))
			{
				specularLo = 0.f;
			}
	
			// demodulate specular
			specularLo /= max((f0 * integratedBRDF.x + integratedBRDF.y), 0.01f);

			diffuseLo = luminance * lobes.diffuse / brdf_utils::lambert::Evaluate(max(surface.albedo, 0.001f));
		}

		CONSTS->specularLumHitDistanceTexture[pixel] = float4(specularLo, hitDistance);
		CONSTS->diffuseLumHitDistanceTexture[pixel]  = float4(diffuseLo, hitDistance);
		CONSTS->lightDirectionTexture[pixel]         = OctahedronEncodeNormal(lightDir);
	}
}
