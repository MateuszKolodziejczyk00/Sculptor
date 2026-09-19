#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(CloudscapeConstants, PARAMS_CLOUDSCAPE)]]
[[shader_params(WeatherMapPaintCommandConstants, PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void WeatherMapPaintCommandCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;
	if (any(coords >= PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->resolution))
	{
		return;
	}

	const float2 mouseUV = PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->mouseUV;
	const Ray mouseRay = CreateViewRayWS(VIEW->sceneView, mouseUV);

	CloudscapeConstants cloudscape = *PARAMS_CLOUDSCAPE;

	const Sphere atmosphereSphere = Sphere::Create(float3(0.f, 0.f, cloudscape.cloudsAtmosphereCenterZ), cloudscape.cloudsAtmosphereInnerRadius);

	const IntersectionResult mouseIntersection = mouseRay.IntersectSphere(atmosphereSphere);
	if (mouseIntersection.IsValid())
	{
		const float3 mouseIntersectionLocation = mouseRay.origin + mouseRay.direction * mouseIntersection.GetTime();

		const float2 weatherMapUV = (coords + 0.5f) / PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->resolution;
		const float2 pixelLocation = (weatherMapUV - 0.5f) / PARAMS_CLOUDSCAPE->weatherMapScale;

		const uint channelToPaint = PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->channelToPaint;

		float noise = 1.f;
		if (channelToPaint == 1u) //clouds type
		{
			noise = PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->weatherMapNoise.SampleLevel(BindlessSamplers::LinearRepeat(), pixelLocation / 37000.f, 0.f).r;
			noise = saturate((noise - 0.25f) / 0.75f);
		}
		else if (channelToPaint == 2u) // bottom type
		{
			noise = PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->weatherMapNoise.SampleLevel(BindlessSamplers::LinearRepeat(), pixelLocation / 1900.f, 0.f).r;
		}
		else if (channelToPaint == 3u)
		{
			//noise = PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->weatherMapNoise.SampleLevel(BindlessSamplers::LinearRepeat(), pixelLocation / 37000.f, 0.f).r;
			//noise = saturate((noise - 0.1f) * 1.3f);
			noise = 1.f;
		}

		if (length(mouseIntersectionLocation.xy - pixelLocation) < PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->radius)
		{
			float brushStrength = saturate((1.f - length(mouseIntersectionLocation.xy - pixelLocation) / PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->radius));
			if (channelToPaint != 0u)
			{
				brushStrength *= noise;
			}

			float4 weatherMapValue = PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->rwWeatherMap.Load(coords);

			float newValue = weatherMapValue[channelToPaint] + PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->paintValue * brushStrength;

			if (channelToPaint != 0u)
			{
				newValue = min(newValue, noise);
			}

			weatherMapValue[channelToPaint] = saturate(newValue);

			PARAMS_WEATHER_MAP_PAINT_COMMAND_CONSTANTS->rwWeatherMap.Store(coords, weatherMapValue);
		}
	}
}
