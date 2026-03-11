#include "CloudsNoiseTexturesGenerator.h"
#include "ResourcesManager.h"
#include "Types/Texture.h"
#include "JobSystem.h"
#include "Utility/Noise.h"
#include "MathUtils.h"


namespace spt::rsc::clouds
{

CloudsNoiseData ComputeBaseShapeNoiseTextureWorley()
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution(128u, 128u, 128u);
	const rhi::EFragmentFormat format = rhi::EFragmentFormat::R8_UN_Float;

	rhi::TextureDefinition textureDef;
	textureDef.resolution = resolution;
	textureDef.format     = format;

	lib::DynamicArray<Byte> data(resolution.x() * resolution.y() * resolution.z());

	js::LaunchInline(SPT_GENERIC_JOB_NAME,
					 [&data, resolution]()
					 {
						const Uint32 rowStride   = resolution.x() * sizeof(Uint8);
						const Uint32 depthStride = resolution.y() * rowStride;

						 for (Uint32 z = 0u; z < resolution.z(); ++z)
						 {
							 for (Uint32 y = 0u; y < resolution.y(); ++y)
							 {
								 js::AddNested(SPT_GENERIC_JOB_NAME,
											   [&data, z, y, resolution, rowStride, depthStride]()
											   {
												   for (Uint32 x = 0u; x < resolution.x(); ++x)
												   {
													   const math::Vector3f coords = (math::Vector3u(x, y, z).cast<Real32>() + math::Vector3f::Constant(0.5f)).cwiseQuotient(resolution.cast<Real32>());

													   const Int32 octaveNum = 3;
													   const Real32 frequency = 8.f;
													   const Real32 perlinNoise = lib::noise::GenerateTileablePerlineNoise3D(coords, frequency, octaveNum);

													   Real32 perlinWorleyNoise = 0.f;
													   {
														   const Real32 worleyCellCount     = 4.f;
														   constexpr Uint32 worleyOctaveNum = 3u;

														   const Real32 worleyFrequencies[worleyOctaveNum] = { 2.f, 8.f, 14.f };

														   Real32 worleyOctaves[worleyOctaveNum] = {};
														   for (Uint32 i = 0u; i < worleyOctaveNum; ++i)
														   {
															   const Real32 worleyFrequency = worleyFrequencies[i];

															   worleyOctaves[i] = (1.f - lib::noise::GenerateTileableWorleyNoise3D(coords, worleyCellCount * worleyFrequency));
														   }

														   const Real32 worleyFBM = worleyOctaves[0] * 0.625f + worleyOctaves[1] * 0.25f + worleyOctaves[2] * 0.125f;

														   perlinWorleyNoise = math::Utils::Remap(perlinNoise, 1.f - worleyFBM, 1.f, 0.f, 1.f);
													   }

													   const Real32 worleyCellCount = 4.f;
													   constexpr Uint32 worleyOctaveNum = 4u;

													   const Real32 worleyFrequencies[worleyOctaveNum] = { 2.f, 4.f, 8.f, 16.f };

													   Real32 worleyOctaves[worleyOctaveNum] = {};
													   for (Uint32 i = 0u; i < worleyOctaveNum; ++i)
													   {
														   const Real32 worleyFrequency = worleyFrequencies[i];

														   worleyOctaves[i] = (1.f - lib::noise::GenerateTileableWorleyNoise3D(coords, worleyCellCount * worleyFrequency));
													   }

													   const Real32 worleyFBM0 = worleyOctaves[0] * 0.625f + worleyOctaves[1] * 0.25f + worleyOctaves[2] * 0.125f;
													   const Real32 worleyFBM1 = worleyOctaves[1] * 0.625f + worleyOctaves[2] * 0.25f + worleyOctaves[3] * 0.125f;
													   const Real32 worleyFBM2 = worleyOctaves[2] * 0.75f + worleyOctaves[3] * 0.25f;

													   const Real32 lowFreqFBM = worleyFBM0 * 0.625f + worleyFBM1 * 0.25f + worleyFBM2 * 0.125f;
													   const Real32 baseCloud = perlinWorleyNoise;

													   Real32 noiseValue = math::Utils::Remap(baseCloud, -(1.f - lowFreqFBM), 1.f, 0.f, 1.f);

													   noiseValue = std::min(1.f, std::max(0.f, noiseValue));

													   const Byte noiseValueUint8 = static_cast<Byte>(noiseValue * 255.0f);

													   data[z * depthStride + rowStride * y + x] = noiseValueUint8;
												   }
											   });
							 }
						 }
					 });

	return CloudsNoiseData
	{
		.resolution = resolution,
		.format     = format,
		.linearData = std::move(data)
	};
}

CloudsNoiseData ComputeDetailShapeNoiseTextureWorley()
{

	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution(32u, 32u, 32u);
	const rhi::EFragmentFormat format = rhi::EFragmentFormat::R8_UN_Float;

	rhi::TextureDefinition textureDef;
	textureDef.resolution = resolution;
	textureDef.format     = format;

	lib::DynamicArray<Byte> data(resolution.x() * resolution.y() * resolution.z());

	js::LaunchInline(SPT_GENERIC_JOB_NAME,
					 [&data, resolution]()
					 {
						const Uint32 rowStride   = resolution.x() * sizeof(Uint8);
						const Uint32 depthStride = resolution.y() * rowStride;

						 for (Uint32 z = 0u; z < resolution.z(); ++z)
						 {
							 for (Uint32 y = 0u; y < resolution.y(); ++y)
							 {
								 js::AddNested(SPT_GENERIC_JOB_NAME,
											   [&data, z, y, resolution, rowStride, depthStride]()
											   {
												   for (Uint32 x = 0u; x < resolution.x(); ++x)
												   {
													   const math::Vector3f coords = (math::Vector3u(x, y, z).cast<Real32>() + math::Vector3f::Constant(0.5f)).cwiseQuotient(resolution.cast<Real32>());

													   const Real32 cellCount = 2.f;

													   constexpr Uint32 worleyOctaveNum = 4u;
													   const Real32 worleyFrequencies[worleyOctaveNum] = { 1.f, 2.f, 4.f, 8.f };

													   Real32 worleyOctaves[worleyOctaveNum] = {};
													   for (Uint32 octaveIdx = 0u; octaveIdx < worleyOctaveNum; ++octaveIdx)
													   {
														   const Real32 freq = worleyFrequencies[octaveIdx];
														   worleyOctaves[octaveIdx] = (1.f - lib::noise::GenerateTileableWorleyNoise3D(coords, cellCount * freq));
													   }

													   const Real32 worleyFBM0 = worleyOctaves[0] * 0.625f + worleyOctaves[1] * 0.25f + worleyOctaves[2] * 0.125f;
													   const Real32 worleyFBM1 = worleyOctaves[1] * 0.625f + worleyOctaves[2] * 0.25f + worleyOctaves[3] * 0.125f;
													   const Real32 worleyFBM2 = worleyOctaves[2] * 0.750f + worleyOctaves[3] * 0.25f;

													   Real32 noiseValue = worleyFBM0 * 0.625f + worleyFBM1 * 0.25f + worleyFBM2 * 0.125f;

													   const Byte noiseValueUint8 = static_cast<Byte>(noiseValue * 255.0f);

													   data[z * depthStride + rowStride * y + x] = noiseValueUint8;
												   }
											   });
							 }
						 }
					 });

	return CloudsNoiseData
	{
		.resolution = resolution,
		.format     = format,
		.linearData = std::move(data)
	};
}

} // spt::rsc::clouds
