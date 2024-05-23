#pragma once

#include "SculptorCoreTypes.h"
#include "DDGITypes.h"
#include "Utility/NamedType.h"
#include "RHICore/RHIAllocationTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "RenderSceneRegistry.h"

namespace spt::rdr
{
class Buffer;
} // spt::rdr


namespace spt::rsc
{

class RenderScene;
class SceneView;
class RenderView;


namespace ddgi
{

class DDGIScene;
class DDGIVolume;
class DDGIZonesCollector;
struct DDGIConfig;


class DDGILOD
{
public:

	DDGILOD();

	void Initialize(DDGIScene& scene, const DDGIConfig& config, Uint32 lodLevel);
	void Deinitialize(DDGIScene& scene);

	void Update(DDGIScene& scene, const SceneView& mainView);

	const DDGIVolume& GetVolume() const;

private:

	Uint32 m_lodLevel = idxNone<Uint32>;

	lib::SharedPtr<DDGIVolume> m_volume;

	Real32 m_forwardAlignment = 0.f;
	Real32 m_heightAlignment  = 0.f;
};


class DDGIScene
{
public:

	explicit DDGIScene(RenderScene& renderScene);
	~DDGIScene();

	void Initialize(const DDGIConfig& config);

	void Update(const SceneView& mainView);

	void CollectZonesToRelit(DDGIZonesCollector& zonesCollector) const;

	void PostRelit();

	const lib::DynamicArray<DDGIVolume*>& GetVolumes() const;

	const lib::MTHandle<DDGISceneDS>& GetDDGIDS() const;

	const Uint32   GetLODsNum() const;
	const DDGILOD& GetLOD(Uint32 lod) const;

	void RegisterVolume(DDGIVolume& volume);
	void UnregisterVolume(DDGIVolume& volume);
	
	lib::SharedRef<DDGIVolume> BuildVolume(const DDGIVolumeParams& params);

	RenderScene& GetOwningScene() const;

private:

	// Volume Initialization

	Uint32                     FindAvailableVolumeIdx() const;

	DDGIGPUVolumeHandle        CreateGPUVolume(const DDGIVolumeParams& params);
	DDGIVolumeGPUDefinition    CreateGPUDefinition(const DDGIVolumeParams& params) const;

	// LODs

	void InitializeLODs(const DDGIConfig& config);
	void DeinitializeLODs();

	// Update

	void UpdateLODs(const SceneView& mainView);
	void FlushDataChanges();

	void UpdatePriorities(const SceneView& mainView);

	// Callbacks

	void OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity);

	RenderScene& m_owningScene;

	lib::MTHandle<DDGISceneDS> m_ddgiSceneDS;

	lib::DynamicArray<DDGIVolume*> m_volumes;

	lib::DynamicArray<DDGILOD> m_lods;
};

} // ddgi

} // spt::rsc