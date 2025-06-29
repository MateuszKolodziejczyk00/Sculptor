#pragma once

#include "SculptorCoreTypes.h"
#include "GraphicsMacros.h"
#include "RGResources/RGResourceHandles.h"
#include "Math/Sequences.h"
#include "Math/MathUtils.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::gfx
{

struct TemporalAAInitSettings
{

};


enum class ETemporalAAQuality : Uint8
{
	Low,
	Medium,
	Ultra,

	Default = Ultra
};

enum class ETemporalAAFlags : Uint8
{
	None = 0u,
	HDR           = BIT(0),
	DepthInverted = BIT(1),
	MotionLowRes  = BIT(2),

	Default = None
};


struct TemporalAAParams
{
	math::Vector2u inputResolution  = {};
	math::Vector2u outputResolution = {};

	ETemporalAAQuality quality = ETemporalAAQuality::Default;
	ETemporalAAFlags flags     = ETemporalAAFlags::Default;

	Bool enableUnifiedDenoising = false;

	Bool enableTransformerModel = false;
};


struct TemporalAAExposureParams
{
	rg::RGBufferViewHandle exposureBuffer;
	Uint32 exposureOffset        = 0u;
	Uint32 historyExposureOffset = 0u;
};


struct UnifiedDenoisingParams
{
	math::Matrix4f worldToView = math::Matrix4f::Identity();
	math::Matrix4f viewToClip  = math::Matrix4f::Identity();

	rg::RGTextureViewHandle diffuseAlbedo;
	rg::RGTextureViewHandle specularAlbedo;
	rg::RGTextureViewHandle normals;
	rg::RGTextureViewHandle roughness;
	rg::RGTextureViewHandle specularHitDistance;
};


struct TemporalAARenderingParams
{
	rg::RGTextureViewHandle depth;
	rg::RGTextureViewHandle motion;

	rg::RGTextureViewHandle inputColor;
	rg::RGTextureViewHandle outputColor;

	math::Vector2f jitter = {};

	Real32 sharpness = 0.0f;

	Bool resetAccumulation = false;

	TemporalAAExposureParams exposure;

	std::optional<UnifiedDenoisingParams> unifiedDenoisingParams = std::nullopt;
};


struct TemporalAAJitterSequence
{
	static math::Vector2f Halton(Uint64 frameIdx, math::Vector2u resolution, Uint32 sequenceLength) 
	{
		SPT_CHECK(resolution.x() != 0 && resolution.y() != 0);
		SPT_CHECK(sequenceLength > 1u);
		SPT_CHECK(math::Utils::IsPowerOf2(sequenceLength));

		const Uint32 sequenceIdx = static_cast<Uint32>(frameIdx & (sequenceLength - 1u));
		const math::Vector2f sequenceVal = math::Vector2f(math::Sequences::Halton<Real32>(sequenceIdx, 2), math::Sequences::Halton<Real32>(sequenceIdx, 3));
		return (sequenceVal * 2.f - math::Vector2f::Constant(1.f)).cwiseProduct(resolution.cast<Real32>().cwiseInverse());
	}
};


class GRAPHICS_API TemporalAARenderer
{
public:

	TemporalAARenderer() = default;
	virtual ~TemporalAARenderer() = default;

	virtual math::Vector2f ComputeJitter(Uint64 frameIdx, math::Vector2u renderingResolution, math::Vector2u outputResolution) const = 0;

	virtual Bool Initialize(const TemporalAAInitSettings& initSettings) = 0 { return true; };

	virtual Bool PrepareForRendering(const TemporalAAParams& params) { return true; }

	virtual void Render(rg::RenderGraphBuilder& graphBuilder, const TemporalAARenderingParams& renderingParams) = 0 {};

	lib::StringView GetName() const { return m_name; }

	Bool SupportsUpscaling() const { return m_supportsUpscaling; }

	Bool ExecutesUnifiedDenoising() const { return m_executesUnifiedDenoising; }

protected:

	lib::StringView m_name;

	Bool m_supportsUpscaling        = false;
	Bool m_executesUnifiedDenoising = false;
};

} // spt::gfx