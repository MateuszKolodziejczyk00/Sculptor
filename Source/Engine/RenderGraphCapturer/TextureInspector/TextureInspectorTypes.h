#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Types/Buffer.h"


namespace spt::rg::capture
{

enum class ETextureInspectorVisualizationMode
{
	Color,
	Alpha,
	VisualizeNaNs,
	
	FloatTextureModesCount,
	Hash = FloatTextureModesCount,

	IntTextureModesCount,
};


enum class EInspectedTextureColorSpace
{
	LinearRGB,
	sRGB,
};


BEGIN_SHADER_STRUCT(TextureInspectorFilterParams)
	SHADER_STRUCT_FIELD(math::Vector2i, hoveredPixel)
	SHADER_STRUCT_FIELD(Real32, minValue)
	SHADER_STRUCT_FIELD(Real32, maxValue)
	SHADER_STRUCT_FIELD(Int32,  rChannelVisible)
	SHADER_STRUCT_FIELD(Int32,  gChannelVisible)
	SHADER_STRUCT_FIELD(Int32,  bChannelVisible)
	SHADER_STRUCT_FIELD(Int32,  isIntTexture)
	SHADER_STRUCT_FIELD(Uint32, visualizationMode)
	SHADER_STRUCT_FIELD(Bool,   shouldOutputHistogram)
	SHADER_STRUCT_FIELD(Uint32, depthSlice3D)
	SHADER_STRUCT_FIELD(Uint32, colorSpace)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(TextureInspectorReadbackData)
	SHADER_STRUCT_FIELD(math::Vector4f, hoveredPixelValue)
END_SHADER_STRUCT();


struct TextureHistogram
{
	TextureHistogram() = default;

	explicit TextureHistogram(const rhi::RHIMappedBuffer<math::Vector4u>& histogramBuffer)
	{
		SPT_CHECK(histogramBuffer.GetElementsNum() == binsNum);
		for (SizeType binIdx = 0u; binIdx < binsNum; ++binIdx)
		{
			for(SizeType channelIdx = 0u; channelIdx < channelsNum; ++channelIdx)
			{
				histogram[channelIdx][binIdx] = static_cast<Real32>(histogramBuffer[binIdx][channelIdx]);
			}
		}
	}

	static constexpr SizeType binsNum     = 128u;
	static constexpr SizeType channelsNum = 4u;
	lib::StaticArray<Real32, binsNum> histogram[channelsNum];
};


class TextureInspectorReadback
{
public:

	struct ReadbackPayload
	{
		std::optional<TextureHistogram> histogram;
	};

	TextureInspectorReadback() = default;
	
	TextureInspectorReadbackData GetData() const
	{
		lib::LockGuard lock(m_readbackDataLock);
		const TextureInspectorReadbackData result = m_readbackData;
		return result;
	}

	std::optional<TextureHistogram> GetHistogram() const
	{
		lib::LockGuard lock(m_readbackDataLock);
		return m_histogram;
	}

	void SetData(const TextureInspectorReadbackData& data, const ReadbackPayload& payload)
	{
		lib::LockGuard lock(m_readbackDataLock);
		m_readbackData = data;

		if (payload.histogram)
		{
			m_histogram = *payload.histogram;
		}
	}

private:

	TextureInspectorReadbackData m_readbackData;

	std::optional<TextureHistogram> m_histogram;

	mutable lib::Lock m_readbackDataLock;
};


struct SaveTextureParams
{
	lib::String path;
};


} // spt::rg::capture