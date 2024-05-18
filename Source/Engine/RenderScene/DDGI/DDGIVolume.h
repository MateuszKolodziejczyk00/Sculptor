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

	void PostVolumeRelit();

	void MarkSunDirectionAsDirty();

	math::AlignedBox3f GetPrevAABB() const;

	Bool RequiresInvalidation() const;
	void PostInvalidation();

	Bool WantsFullVolumeRelit() const;

	Bool MovedSinceLastRelit() const;

	DDGIScene& GetDDGIScene() const;

	EDDGIVolumeState GetState() const;
	Bool             IsReady() const;

	Uint32           GetVolumeIdx() const;

	const math::AlignedBox3f& GetVolumeAABB() const;
	math::Vector3f            GetVolumeCenter() const;

	Uint32 GetProbeDataTexturesNum() const;

	lib::SharedPtr<rdr::TextureView> GetProbesIlluminanceTexture(Uint32 textureIdx) const;
	lib::SharedPtr<rdr::TextureView> GetProbesHitDistanceTexture(Uint32 textureIdx) const;
	lib::SharedPtr<rdr::TextureView> GetProbesAverageLuminanceTexture() const;

	const DDGIVolumeGPUParams& GetVolumeGPUParams() const;

	Real32 GetRelitHysteresisDelta() const;

	const math::Vector3u& GetProbesVolumeResolution() const;
	Uint32 GetProbesNum() const;

	void   UpdateRelitPriority(const SceneView& sceneView, Real32 currentTime, Real32 deltaTime);
	Real32 GetRelitPriority() const;

private:

	Real32 ComputeRelitPriorityFactor(const SceneView& sceneView, Real32 currentTime, Real32 deltaTime) const;

	void TranslateImpl(const math::Vector3i& wrapDelta);

	DDGIScene& m_owningScene;

	EDDGIVolumeState m_state = EDDGIVolumeState::Uninitialized;

	math::AlignedBox3f m_volumeAABB;

	DDGIGPUVolumeHandle m_gpuVolumeHandle;

	std::optional<math::AlignedBox3f> m_prevAABB;
	Bool m_requiresInvalidation;

	Real32 m_volumePriority = 0.0f;

	Real32 m_relitPriority = 0.0f;

	Real32 m_lastRelitTime = 0.0f;

	Bool m_isSunLightDirectionDirty = false;
};

} // ddgi

} // spt::rsc