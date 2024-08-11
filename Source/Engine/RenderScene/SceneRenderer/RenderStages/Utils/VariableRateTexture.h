#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "Shaders/ShaderTypes.h"


namespace spt::sc
{
class ShaderCompilationSettings;
} // spt::sc


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rg
{
class RenderGraphBuilder;
class RGDescriptorSetStateBase;
} // spt::rg


namespace spt::rsc
{

namespace vrt
{

enum class EReprojectionFailedMode
{
	_1x1,
	_1x2,
	_2x2,
	_4x4
};


enum class EMaxVariableRate
{
	_2x2 = 2u,
	_4x4 = 4u
};


struct VariableRatePermutationSettings
{
	EMaxVariableRate maxVariableRate = EMaxVariableRate::_2x2;
};


struct VariableRateSettings
{
	VariableRateSettings() = default;

	float xThreshold2 = 0.1f;
	float yThreshold2 = 0.1f;

	float xThreshold4 = 0.0001f;
	float yThreshold4 = 0.0001f;

	// How many numbers are used in single slot
	// Texture has 4 slots
	// Higher number will result in slower reaction to changes but better stability
	Uint32 logFramesNumPerSlot = 2u;

	EReprojectionFailedMode reprojectionFailedMode = EReprojectionFailedMode::_2x2;

	VariableRatePermutationSettings permutationSettings;
};

void ApplyVariableRatePermutation(sc::ShaderCompilationSettings& compilationSettings, const VariableRatePermutationSettings& permutationSettings);

math::Vector2u ComputeVariableRateTextureResolution(const math::Vector2u& inputTextureResolution);

void RenderVariableRateTexture(rg::RenderGraphBuilder& graphBuilder, const VariableRateSettings& vrSettings, rg::RGTextureViewHandle inputTexture, rg::RGTextureViewHandle variableRateTexture, Uint32 frameIdx, std::optional<rdr::ShaderID> customShader, lib::Span<const lib::MTHandle<rg::RGDescriptorSetStateBase>> additionalDescriptorSets);

void ReprojectVariableRateTexture(rg::RenderGraphBuilder& graphBuilder, const VariableRateSettings& vrSettings, rg::RGTextureViewHandle motionTexture, rg::RGTextureViewHandle sourceTexture, rg::RGTextureViewHandle targetTexture);


class VariableRateRenderer
{
public:

	VariableRateRenderer();

	void Initialize(const VariableRateSettings& vrSettings);

	const VariableRatePermutationSettings& GetPermutationSettings() const { return m_vrSettings.permutationSettings; }

	const lib::SharedPtr<rdr::TextureView>& GetVariableRateTexture() const { return m_reprojectionTargetVRTexture; }

	void Reproject(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle motionTexture);

	void Render(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle inputTexture, std::optional<rdr::ShaderID> customShader = {}, lib::Span<const lib::MTHandle<rg::RGDescriptorSetStateBase>> additionalDescriptorSets = {});

private:

	// returns true if reprojection can be performed
	Bool PrepareTextures(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle inputTexture);

	lib::SharedPtr<rdr::TextureView> m_reprojectionSourceVRTexture;
	lib::SharedPtr<rdr::TextureView> m_reprojectionTargetVRTexture;

	VariableRateSettings m_vrSettings;

	Uint32 m_frameIdx = 0u;
};

} // vrt

} // spt::rsc
