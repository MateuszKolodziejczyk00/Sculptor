#pragma once

#include "SculptorCoreTypes.h"
#include "View/ViewRenderingSpec.h"
#include "VolumetricCloudsTypes.h"

namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc::clouds
{

class VolumetricCloudsRenderer
{
public:

	VolumetricCloudsRenderer();

	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs);

	const CloudsTransmittanceMap& GetCloudsTransmittanceMap() const { return m_cloudsTransmittanceMap; }

	Bool AreVolumetricCloudsEnabled() const { return m_volumetricCloudsEnabled; }

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const CloudscapeContext& cloudscapeContext);

	void RenderVolumetricClouds(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context, const CloudscapeContext cloudscapeContext);

	CloudscapeContext CreateFrameCloudscapeContext(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs);

	void UpdateCloudscapeProbes(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const CloudscapeContext& cloudscapeContext);

	void InitTextures();

	void LoadOrCreateBaseShapeNoiseTexture();
	void LoadOrCreateDetailShapeNoiseTexture();
	void LoadWeatherMapTexture();
	void LoadCurlNoiseTexture();
	void LoadDensityLUTTexture();

	Bool m_volumetricCloudsEnabled = false;

	lib::SharedPtr<rdr::TextureView> m_baseShapeNoiseTexture;
	lib::SharedPtr<rdr::TextureView> m_detailShapeNoiseTexture;

	lib::SharedPtr<rdr::TextureView> m_weatherMap;

	lib::SharedPtr<rdr::TextureView> m_densityLUT;

	lib::SharedPtr<rdr::TextureView> m_curlNoise;

	lib::SharedPtr<rdr::TextureView> m_cloudscapeProbes;

	CloudscapeConstants m_cloudscapeConstants;

	CloudsTransmittanceMap m_cloudsTransmittanceMap;
};

} //spt::rsc::clouds