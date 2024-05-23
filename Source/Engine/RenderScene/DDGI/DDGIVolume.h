#pragma once

#include "SculptorCoreTypes.h"
#include "DDGITypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

class SceneView;


namespace ddgi
{

class DDGIScene;
class DDGIVolume;
class DDGIZonesCollector;


class DDGIRelitZone
{
public:

	DDGIRelitZone();

	void Initialize(DDGIVolume& volume, const math::AlignedBox3u& probesBoundingBox);

	DDGIVolume&               GetVolume()            const;
	const math::AlignedBox3u& GetProbesBoundingBox() const;

	Bool   WantsForceRelitNextFrame() const { return m_forceRelitNextFrame; }
	Real32 GetCurrentRelitPriority()  const { return m_currentRelitPriority; }

	math::AlignedBox3f GetBoundingBox() const;

	void MarkSunLightDirectionDirty();

	void UpdateRelitPriority(const SceneView& sceneView, Real32 currentTime, Real32 deltaTime);

	Real32 GetRelitHysteresisDelta() const;

	void PostRelit();

private:

	Real32 ComputeRelitPriorityFactor(const SceneView& sceneView, Real32 currentTime, Real32 deltaTime) const;

	Bool IsOverlappingInvalidProbes() const;

	DDGIVolume* m_volume = nullptr;

	// Can't use default constructor because of unsigned type (it uses unary minus)
	math::AlignedBox3u m_probesBoundingBox = math::AlignedBox3u(math::Vector3u::Constant(999u), math::Vector3u::Constant(999u));

	Bool   m_forceRelitNextFrame  = false;
	Real32 m_currentRelitPriority = 0.0f;

	Bool m_isSunLightDirectionDirty = false;

	Real32 m_lastRelitTime = 0.0f;
};


class DDGIVolumeZones
{
public:

	DDGIVolumeZones();

	void Initialize(DDGIVolume& volume, const math::Vector3u& zoneSize);

	void MarkSunLightDirectionDirty();

	void UpdateRelitPriority(const SceneView& sceneView, Real32 currentTime, Real32 deltaTime);

	void CollectZonesToRelit(DDGIZonesCollector& zonesCollector);

private:

	lib::DynamicArray<DDGIRelitZone> m_zones;
};


class DDGIZonesCollector
{
public:

	explicit DDGIZonesCollector(SizeType zonesToRelitBudget);

	void Collect(DDGIRelitZone& zone);

	const lib::DynamicArray<DDGIRelitZone*>& GetZonesToRelit() const;

private:

	lib::DynamicArray<DDGIRelitZone*> m_zonesToRelit;

	SizeType m_zonesToRelitBudget;
};


enum class EDDGIVolumeState
{
	Uninitialized,
	Ready
};


class DDGIVolume
{
public:

	explicit DDGIVolume(DDGIScene& owningScene);
	~DDGIVolume();

	void Initialize(DDGIGPUVolumeHandle gpuVolumeHandle, const DDGIVolumeParams& inParams);

	Bool Translate(const math::Translation3f& requestedTranslation);
	void TeleportTo(const math::Vector3f& newAABBMin);

	void MarkSunLightDirectionDirty();

	math::AlignedBox3f GetPrevAABB() const;

	math::AlignedBox3f GetZoneAABB(const math::AlignedBox3u& probesAABB) const;

	Bool RequiresInvalidation() const;
	void PostInvalidation();

	Bool MovedSinceLastRelit() const;
	math::AlignedBox3f GetVolumePrevAABB() const;

	DDGIScene& GetDDGIScene() const;

	EDDGIVolumeState GetState() const;
	Bool             IsReady() const;

	Uint32           GetVolumeIdx() const;

	const math::AlignedBox3f& GetVolumeAABB() const;
	math::Vector3f            GetVolumeCenter() const;

	Real32 GetVolumePriority() const;

	Uint32 GetProbeDataTexturesNum() const;

	lib::SharedPtr<rdr::TextureView> GetProbesIlluminanceTexture(Uint32 textureIdx) const;
	lib::SharedPtr<rdr::TextureView> GetProbesHitDistanceTexture(Uint32 textureIdx) const;
	lib::SharedPtr<rdr::TextureView> GetProbesAverageLuminanceTexture() const;

	const DDGIVolumeGPUParams& GetVolumeGPUParams() const;

	const math::Vector3u& GetProbesVolumeResolution() const;
	Uint32 GetProbesNum() const;

	void UpdateRelitPriority(const SceneView& sceneView, Real32 currentTime, Real32 deltaTime);

	void CollectZonesToRelit(DDGIZonesCollector& zonesCollector);

private:

	void TranslateImpl(const math::Vector3i& wrapDelta);

	DDGIScene& m_owningScene;

	EDDGIVolumeState m_state = EDDGIVolumeState::Uninitialized;

	math::AlignedBox3f m_volumeAABB;

	DDGIGPUVolumeHandle m_gpuVolumeHandle;

	std::optional<math::AlignedBox3f> m_prevAABB;
	Bool m_requiresInvalidation;

	Real32 m_volumePriority = 0.0f;

	DDGIVolumeZones m_relitZones;
};

} // ddgi

} // spt::rsc