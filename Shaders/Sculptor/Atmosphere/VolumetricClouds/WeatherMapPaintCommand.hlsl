#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS)]]
[[descriptor_set(CloudscapeDS)]]
[[shader_params(WeatherMapPaintCommandConstants, u_constants)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(16, 16, 1)]
void WeatherMapPaintCommandCS(CS_INPUT input)
{
	const uint2 coords = input.globalID.xy;
	if (any(coords >= u_constants.resolution))
	{
		return;
	}

	const float2 mouseUV = u_constants.mouseUV;
	const Ray mouseRay = CreateViewRayWS(u_sceneView, mouseUV);

	CloudscapeConstants cloudscape = u_cloudscapeConstants;

	const Sphere atmosphereSphere = Sphere::Create(float3(0.f, 0.f, cloudscape.cloudsAtmosphereCenterZ), cloudscape.cloudsAtmosphereInnerRadius);

	const IntersectionResult mouseIntersection = mouseRay.IntersectSphere(atmosphereSphere);
	if (mouseIntersection.IsValid())
	{
		const float3 mouseIntersectionLocation = mouseRay.origin + mouseRay.direction * mouseIntersection.GetTime();

		const float2 weatherMapUV = (coords + 0.5f) / u_constants.resolution;
		const float2 pixelLocation = (weatherMapUV - 0.5f) / u_cloudscapeConstants.weatherMapScale;

		const uint channelToPaint = u_constants.channelToPaint;

		float noise = 1.f;
		if (channelToPaint == 1u) //clouds type
		{
			noise = u_constants.weatherMapNoise.SampleLevel(BindlessSamplers::LinearRepeat(), pixelLocation / 37000.f, 0.f).r;
			noise = saturate((noise - 0.25f) / 0.75f);
		}
		else if (channelToPaint == 2u) // bottom type
		{
			noise = u_constants.weatherMapNoise.SampleLevel(BindlessSamplers::LinearRepeat(), pixelLocation / 1900.f, 0.f).r;
		}
		else if (channelToPaint == 3u)
		{
			//noise = u_constants.weatherMapNoise.SampleLevel(BindlessSamplers::LinearRepeat(), pixelLocation / 37000.f, 0.f).r;
			//noise = saturate((noise - 0.1f) * 1.3f);
			noise = 1.f;
		}

		if (length(mouseIntersectionLocation.xy - pixelLocation) < u_constants.radius)
		{
			float brushStrength = saturate((1.f - length(mouseIntersectionLocation.xy - pixelLocation) / u_constants.radius));
			if (channelToPaint != 0u)
			{
				brushStrength *= noise;
			}

			float4 weatherMapValue = u_constants.rwWeatherMap.Load(coords);

			float newValue = weatherMapValue[channelToPaint] + u_constants.paintValue * brushStrength;

			if (channelToPaint != 0u)
			{
				newValue = min(newValue, noise);
			}

			weatherMapValue[channelToPaint] = saturate(newValue);

			u_constants.rwWeatherMap.Store(coords, weatherMapValue);
		}
	}
}
