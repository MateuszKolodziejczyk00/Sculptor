#include "SculptorShader.hlsli"

[[shader_params(ResolveReservoirsParams, PARAMS_RESOLVE_RESERVOIRS)]]
[[shader_params(GPURenderView, VIEW)]]

#include "SpecularReflections/SRReservoir.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Shading/Shading.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"

struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void ResolveReservoirsCS(CS_INPUT input)
{
	uint2 pixel = input.globalID.xy;
	
	if(all(pixel < PARAMS_RESOLVE_RESERVOIRS->resamplingConstants->resolution))
	{
		uint2 reservoirCoords = pixel;

		const uint reservoirIdx = GetScreenReservoirIdx(pixel, PARAMS_RESOLVE_RESERVOIRS->resamplingConstants->reservoirsResolution);
		SRReservoir reservoir = UnpackReservoir(PARAMS_RESOLVE_RESERVOIRS->reservoirsBuffer[reservoirIdx]);

		const uint2 traceCoords = GetVariableTraceCoords(PARAMS_RESOLVE_RESERVOIRS->variableRateBlocksTexture, pixel);

		const bool isTracingCoord = all(pixel == traceCoords);

		const bool canUseReservoir = reservoir.IsValid() && reservoir.HasValidResult();

		if(!canUseReservoir && !isTracingCoord)
		{
			reservoirCoords = traceCoords;
			const uint tracingReservoirIdx = GetScreenReservoirIdx(traceCoords, PARAMS_RESOLVE_RESERVOIRS->resamplingConstants->reservoirsResolution);
			reservoir = UnpackReservoir(PARAMS_RESOLVE_RESERVOIRS->reservoirsBuffer[tracingReservoirIdx]);
		}

		float3 specularLo = 0.f;
		float3 diffuseLo  = 0.f;
		float3 lightDir  = 0.f;
		float hitDistance = SPT_NAN;

		if(reservoir.IsValid() && reservoir.HasValidResult())
		{
			const float depth = PARAMS_RESOLVE_RESERVOIRS->depthTexture.Load(uint3(reservoirCoords, 0));
			const float2 uv = (reservoirCoords + 0.5f) * PARAMS_RESOLVE_RESERVOIRS->resamplingConstants->pixelSize;
			const float3 ndc = float3(uv * 2.f - 1.f, depth);
	
			const float3 sampleLocation = NDCToWorldSpace(ndc, VIEW->sceneView);
	
			const float3 sampleNormal = OctahedronDecodeNormal(PARAMS_RESOLVE_RESERVOIRS->normalsTexture.Load(uint3(reservoirCoords, 0)));
	
			const float3 toView = normalize(VIEW->sceneView.viewLocation - sampleLocation);
	
			const float4 props = PARAMS_RESOLVE_RESERVOIRS->propsTexture.Load(uint3(reservoirCoords, 0));
			const float roughness = props.x;
			const float clearCoatWeight = props.y;
			const float clearCoatRoughness = props.z;

			const float4 baseColorMetallic = PARAMS_RESOLVE_RESERVOIRS->baseColorTexture.Load(uint3(reservoirCoords, 0));

			float3 f0;
			float3 diffuseColor;
			ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT f0);

			hitDistance = length(reservoir.hitLocation - sampleLocation);
			lightDir = (reservoir.hitLocation - sampleLocation) / hitDistance;

			const float NdotL = saturate(dot(sampleNormal, lightDir));
	
			float3 luminance = reservoir.luminance;

			if (!reservoir.HasFlag(SR_RESERVOIR_FLAGS_MISS))
			{
				luminance *= EvaluateHeightBasedTransmittanceForSegment(PARAMS_RESOLVE_RESERVOIRS->heightFogParams, sampleLocation, reservoir.hitLocation);
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
			const float2 integratedBRDF = PARAMS_RESOLVE_RESERVOIRS->brdfIntegrationLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(NdotV, roughness), 0);
	
			if(any(isnan(specularLo)) || any(isinf(specularLo)))
			{
				specularLo = 0.f;
			}
	
			// demodulate specular
			specularLo /= max((f0 * integratedBRDF.x + integratedBRDF.y), 0.01f);

			diffuseLo = luminance * lobes.diffuse / brdf_utils::lambert::Evaluate(max(surface.albedo, 0.001f));
		}

		PARAMS_RESOLVE_RESERVOIRS->specularLumHitDistanceTexture[pixel] = float4(specularLo, hitDistance);
		PARAMS_RESOLVE_RESERVOIRS->diffuseLumHitDistanceTexture[pixel]  = float4(diffuseLo, hitDistance);
		PARAMS_RESOLVE_RESERVOIRS->lightDirectionTexture[pixel]         = OctahedronEncodeNormal(lightDir);
	}
}
