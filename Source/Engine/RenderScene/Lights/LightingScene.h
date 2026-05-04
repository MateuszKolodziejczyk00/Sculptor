#pragma once

#include "SculptorCoreTypes.h"
#include "LightTypes.h"
#include "Containers/PagedGenerationalPool.h"


namespace spt::rsc
{

using PointLightHandle = lib::PagedGenerationalPool<PointLightData>::Handle;
using SpotLightHandle  = lib::PagedGenerationalPool<SpotLightData>::Handle;


struct LocalLightHandle
{
public:

	LocalLightHandle() = default;

	LocalLightHandle(PointLightHandle pointLightHandle)
		: m_idx(pointLightHandle.idx)
		, m_generation(pointLightHandle.generation)
	{ }

	LocalLightHandle(SpotLightHandle spotLightHandle)
		: m_idx(spotLightHandle.idx | (1u << 31))
		, m_generation(spotLightHandle.generation)
	{ }

	Bool IsValid() const { return m_idx != idxNone<Uint32>; }

	Bool IsPointLight() const { return (m_idx & (1u << 31)) == 0; }
	Bool IsSpotLight() const { return (m_idx & (1u << 31)) != 0; }

	PointLightHandle AsPointLight() const
	{
		SPT_CHECK(IsPointLight());
		return PointLightHandle{ m_idx, m_generation };
	}

	SpotLightHandle AsSpotLight() const
	{
		SPT_CHECK(IsSpotLight());
		return SpotLightHandle{ m_idx & ~(1u << 31), m_generation };
	}

	std::strong_ordering operator<=>(const LocalLightHandle& other) const = default;

private:

	Uint32 m_idx = idxNone<Uint32>;
	Uint16 m_generation = 0;
};


struct LightingScene
{
public:

	LightingScene()
		: pointLights("LightingScene_PointLightsPool")
		, spotLights("LightingScene_SpotLightsPool")
	{
	}

	void SetDirectionalLight(const DirectionalLightData& directionalLight)
	{
		m_directionalLight = directionalLight;
		m_isDirectionalLightDirty = true;
	}

	const DirectionalLightData& GetDirectionalLight() const { return m_directionalLight; }
	Bool IsDirectionalLightDirty() const { return m_isDirectionalLightDirty; }

	void EndFrame()
	{
		m_isDirectionalLightDirty = false;
	}

	lib::PagedGenerationalPool<PointLightData> pointLights;
	lib::PagedGenerationalPool<SpotLightData>  spotLights;

private:

	DirectionalLightData m_directionalLight;
	Bool                 m_isDirectionalLightDirty = false;
};

} // spt::rsc
