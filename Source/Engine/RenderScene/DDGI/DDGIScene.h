#pragma once

#include "SculptorCoreTypes.h"
#include "DDGITypes.h"
#include "Utility/NamedType.h"
#include "RHICore/RHIAllocationTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ArrayOfSRVTexturesBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/StructuredCPUToGPUBufferBinding.h"
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
struct DDGIConfig;


class DDGILOD0
{
public:

	DDGILOD0();

	void Initialize(DDGIScene& scene, const DDGIConfig& config);
	void Deinitialize(DDGIScene& scene);

	void Update(DDGIScene& scene, const SceneView& mainView);

	const DDGIVolume& GetVolume() const;

private:

	lib::SharedPtr<DDGIVolume> m_volume;
};


class DDGILOD1
{
public:

	DDGILOD1();

	void Initialize(DDGIScene& scene, const DDGIConfig& config);
	void Deinitialize(DDGIScene& scene);

	void Update(DDGIScene& scene, const SceneView& mainView);

private:

	Bool UpdateVolumesLocation(const math::Vector3f& desiredCenter);
	void UpdateLODDefinition(DDGIScene& scene);

	using DDGIGrid = lib::StaticArray<lib::StaticArray<lib::SharedPtr<DDGIVolume>, 3>, 3>;

	DDGIGrid m_volumes;

	math::Vector3f m_singleVolumeSize;
};


class DDGIScene
{
public:

	explicit DDGIScene(RenderScene& renderScene);
	~DDGIScene();

	void Initialize(const DDGIConfig& config);

	void Update(const SceneView& mainView);

	void PostRelit();

	const lib::DynamicArray<DDGIVolume*>& GetVolumes() const;

	const lib::MTHandle<DDGISceneDS>& GetDDGIDS() const;

	const DDGILOD0& GetLOD0() const;
	const DDGILOD1& GetLOD1() const;

	void RegisterVolume(DDGIVolume& volume);
	void UnregisterVolume(DDGIVolume& volume);
	
	lib::SharedRef<DDGIVolume> BuildVolume(const DDGIVolumeParams& params);

private:

	// Volume Initialization

	Uint32                     FindAvailableVolumeIdx() const;

	DDGIGPUVolumeHandle        CreateGPUVolume(const DDGIVolumeParams& params);
	DDGIVolumeGPUParams        CreateGPUData(const DDGIVolumeParams& params) const;

	// LODs

	void InitializeLODs(const DDGIConfig& config);
	void DeinitializeLODs();

	// Update

	void UpdateLODs(const SceneView& mainView);
	void FlushDataChanges();

	void UpdatePriorities(const SceneView& mainView);
	void SortVolumes();

	// Callbacks

	void OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity);

	RenderScene& m_owningScene;

	lib::MTHandle<DDGISceneDS> m_ddgiSceneDS;

	lib::DynamicArray<DDGIVolume*> m_volumes;

	DDGILOD0 m_lod0;
	DDGILOD1 m_lod1;
};

} // ddgi

} // spt::rsc