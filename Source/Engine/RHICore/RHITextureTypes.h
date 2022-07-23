#pragma once


namespace spt::rhi
{

namespace ETextureUsage
{

enum Flags : Flags32
{
	TransferSource				= BIT(0),
	TransferDest				= BIT(1),
	SampledTexture				= BIT(2),
	StorageTexture				= BIT(3),
	ColorRT						= BIT(4),
	DepthSetncilRT				= BIT(5),
	TransientRT					= BIT(6),
	InputRT						= BIT(7),
	ShadingRateTexture			= BIT(8),
	FragmentDensityMap			= BIT(9),
};

}

enum class EFragmentFormat : Uint32
{
	RGBA8_UN_Float,
	RGBA32_S_Float,
	D32_S_Float,
};


enum class ETextureLayout : Uint32
{
	Invalid,
	Preinitialized,
	General
};


struct TextureDefinition
{
	TextureDefinition()
		: m_resolution{}
		, m_usage(0)
		, m_format(EFragmentFormat::RGBA8_UN_Float)
		, m_samples(1)
		, m_mipLevels(1)
		, m_arrayLayers(1)
	{ }

	math::Vector3u		m_resolution;
	Flags32				m_usage;
	EFragmentFormat		m_format;
	Uint8				m_samples;
	Uint8				m_mipLevels;
	Uint8				m_arrayLayers;
};

}