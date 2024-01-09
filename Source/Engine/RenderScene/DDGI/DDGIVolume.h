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

	void RequestGlobalRelit();

	math::AlignedBox3f GetPrevAABB() const;

	Bool RequiresInvalidation() const;
	void PostInvalidation();

	Bool WantsFullVolumeRelit() const;

	Bool MovedSinceLastRelit() const;

	DDGIScene& GetOwningScene() const;

	EDDGIVolumeState GetState() const;
	Bool             IsReady() const;

	Uint32           GetVolumeIdx() const;

	const math::AlignedBox3f& GetVolumeAABB() const;
	math::Vector3f            GetVolumeCenter() const;

	const lib::SharedPtr<rdr::TextureView>& GetProbesIlluminanceTexture() const;
	const lib::SharedPtr<rdr::TextureView>& GetProbesHitDistanceTexture() const;

	const DDGIVolumeGPUParams& GetVolumeGPUParams() const;

	const math::Vector3u& GetProbesVolumeResolution() const;
	Uint32 GetProbesNum() const;

	void   UpdateRelitPriority(const SceneView& sceneView, Real32 deltaTime);
	Real32 GetRelitPriority() const;

private:

	void TranslateImpl(const math::Vector3i& wrapDelta);

	DDGIScene& m_owningScene;

	EDDGIVolumeState m_state = EDDGIVolumeState::Uninitialized;

	math::AlignedBox3f m_volumeAABB;

	DDGIGPUVolumeHandle m_gpuVolumeHandle;

	lib::SharedPtr<rdr::TextureView> m_cachedProbesIlluminanceTexture;
	lib::SharedPtr<rdr::TextureView> m_cachedProbesHitDistanceTexture;

	std::optional<math::AlignedBox3f> m_prevAABB;
	Bool m_requiresInvalidation;

	Real32 m_volumePriority = 0.0f;

	Real32 m_relitPriority = 0.0f;

	Bool m_wantsGlobalRelit = false;
};

} // ddgi

} // spt::rsc