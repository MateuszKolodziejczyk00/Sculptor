#include "DDGIScene.h"
#include "DDGIVolume.h"
#include "Transfers/UploadUtils.h"
#include "ResourcesManager.h"
#include "View/SceneView.h"
#include "RenderScene.h"
#include "Lights/LightTypes.h"
#include "EngineFrame.h"
#include "DDGISceneSubsystem.h"

namespace spt::rsc::ddgi
{

SPT_DEFINE_LOG_CATEGORY(DDGIScene, true);

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGILOD =======================================================================================

DDGILOD::DDGILOD()
{ }

void DDGILOD::Initialize(DDGIScene& scene, const DDGIConfig& config, Uint32 lodLevel, DDGILODDefinition& outLODDef)
{
	SPT_CHECK(m_lodLevel == idxNone<Uint32>);

	m_lodLevel = lodLevel;

	const DDGILODConfig lodConfig = config.lodsConfigs[lodLevel];

	DDGIVolumeParams params;
	params.probesVolumeResolution        = lodConfig.volumeResolution;
	params.relitZoneResolution           = lodConfig.relitZoneResolution;
	params.probesOriginWorldLocation     = math::Vector3f::Constant(0.f);
	params.probesSpacing                 = lodConfig.probesSpacing;
	params.probeIlluminanceDataRes       = config.probeIlluminanceDataRes;
	params.probeHitDistanceDataRes       = config.probeHitDistanceDataRes;
	params.probeIlluminanceEncodingGamma = 1.f;
	params.priority                      = lodConfig.relitPriority;

	m_forwardAlignment = lodConfig.forwardAlignment;
	m_heightAlignment  = lodConfig.heightAlignment;

	m_volume = scene.BuildVolume(params);

	outLODDef.volumeIdx = m_volume->GetVolumeIdx();
}

void DDGILOD::Deinitialize(DDGIScene& scene)
{
	m_volume.reset();
}

void DDGILOD::Update(DDGIScene& scene, const SceneView& mainView)
{
	const math::AlignedBox3f& volumeAABB = m_volume->GetVolumeAABB();
	const math::Vector3f volumeSize      = volumeAABB.sizes();
	const math::Vector3f volumeSize2D    = math::Vector3f(volumeSize.x(), volumeSize.y(), 0.f);

	const Real32 forwardVectorMultiplier = m_forwardAlignment * 0.5f;
	const Real32 heightMultiplier        = m_heightAlignment * 0.5f;
	const math::Vector3f newCenter = mainView.GetLocation() + mainView.GetForwardVector().cwiseProduct(volumeSize2D) * forwardVectorMultiplier + math::Vector3f(0.0f, 0.0f, volumeSize.z() * heightMultiplier);

	m_volume->Translate(math::Translation3f(newCenter - volumeAABB.center()));
}

const DDGIVolume& DDGILOD::GetVolume() const
{
	return *m_volume;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIScene =====================================================================================

DDGIScene::DDGIScene(RenderScene& renderScene)
	: m_owningScene(renderScene)
	, m_ddgiSceneDS(rdr::ResourcesManager::CreateDescriptorSetState<DDGISceneDS>(RENDERER_RESOURCE_NAME("DDGI Scene DS")))
{
	RenderSceneRegistry& registry = m_owningScene.GetRegistry();

	registry.on_construct<DirectionalLightData>().connect<&DDGIScene::OnDirectionalLightUpdated>(this);
	registry.on_update<DirectionalLightData>().connect<&DDGIScene::OnDirectionalLightUpdated>(this);
	registry.on_destroy<DirectionalLightData>().connect<&DDGIScene::OnDirectionalLightUpdated>(this);
}

DDGIScene::~DDGIScene()
{
	DeinitializeLODs();

	RenderSceneRegistry& registry = m_owningScene.GetRegistry();

	registry.on_construct<DirectionalLightData>().disconnect<&DDGIScene::OnDirectionalLightUpdated>(this);
	registry.on_update<DirectionalLightData>().disconnect<&DDGIScene::OnDirectionalLightUpdated>(this);
	registry.on_destroy<DirectionalLightData>().disconnect<&DDGIScene::OnDirectionalLightUpdated>(this);
}

void DDGIScene::Initialize(const DDGIConfig& config)
{
	InitializeLODs(config);
}

void DDGIScene::Update(const SceneView& mainView)
{
	SPT_PROFILER_FUNCTION();

	UpdateLODs(mainView);

	UpdatePriorities(mainView);

	m_ddgiSceneDS->u_ddgiLODs   = m_ddgiLODsDef;
	m_ddgiSceneDS->u_volumesDef = m_ddgiVolumesDef;
}

void DDGIScene::CollectZonesToRelit(DDGIZonesCollector& zonesCollector) const
{
	for (DDGIVolume* volume : GetVolumes())
	{
		volume->CollectZonesToRelit(zonesCollector);
	}
}

void DDGIScene::PostRelit()
{
}

const lib::DynamicArray<DDGIVolume*>& DDGIScene::GetVolumes() const
{
	return m_volumes;
}

const lib::MTHandle<DDGISceneDS>& DDGIScene::GetDDGIDS() const
{
	return m_ddgiSceneDS;
}

const Uint32 DDGIScene::GetLODsNum() const
{
	return static_cast<Uint32>(m_lods.size());
}

const DDGILOD& DDGIScene::GetLOD(Uint32 lod) const
{
	return m_lods[lod];
}

void DDGIScene::RegisterVolume(DDGIVolume& volume)
{
	SPT_CHECK(std::find(std::cbegin(m_volumes), std::cend(m_volumes), &volume) == std::cend(m_volumes));
	m_volumes.emplace_back(&volume);
}

void DDGIScene::UnregisterVolume(DDGIVolume& volume)
{
	auto it = std::find(std::cbegin(m_volumes), std::cend(m_volumes), &volume);
	SPT_CHECK(it != std::cend(m_volumes));
	m_volumes.erase(it);
}

lib::SharedRef<DDGIVolume> DDGIScene::BuildVolume(const DDGIVolumeParams& params)
{
	lib::SharedRef<DDGIVolume> volume = lib::MakeShared<DDGIVolume>(*this);

	const DDGIGPUVolumeHandle gpuVolumeHandle = CreateGPUVolume(params);
	SPT_CHECK(gpuVolumeHandle.IsValid());

	volume->Initialize(gpuVolumeHandle, params);

	return volume;
}

RenderScene& DDGIScene::GetOwningScene() const
{
	return m_owningScene;
}

Uint32 DDGIScene::FindAvailableVolumeIdx() const
{
	for (SizeType i = 0; i < m_ddgiVolumesDef.volumes.size(); ++i)
	{
		if (m_ddgiVolumesDef.volumes[i].isValid == false)
		{
			return static_cast<Uint32>(i);
		}
	}

	return idxNone<Uint32>;
}

DDGIGPUVolumeHandle DDGIScene::CreateGPUVolume(const DDGIVolumeParams& params)
{
	SPT_PROFILER_FUNCTION();

	const Uint32 volumeIdx = FindAvailableVolumeIdx();
	
	if (volumeIdx == idxNone<Uint32>)
	{
		return DDGIGPUVolumeHandle();
	}

	const DDGIVolumeGPUDefinition volumeGPUDefinition = CreateGPUDefinition(params);

	DDGIGPUVolumeHandle newVolumeHandle(m_ddgiSceneDS, volumeIdx, m_ddgiVolumesDef.volumes[volumeIdx], volumeGPUDefinition);

	return newVolumeHandle;
}

DDGIVolumeGPUDefinition DDGIScene::CreateGPUDefinition(const DDGIVolumeParams& params) const
{
	const math::Vector3u probesVolumeRes = params.probesVolumeResolution;

	const Uint32 probeDataTexturesNum = probesVolumeRes.z();

	const Uint32 probesTextureWidth  = probesVolumeRes.x();
	const Uint32 probesTextureHeight = probesVolumeRes.y();

	const math::Vector3f volumeSize = (probesVolumeRes - math::Vector3u::Constant(1u)).cast<Real32>().cwiseProduct(params.probesSpacing);

	DDGIVolumeGPUParams gpuParams;
	gpuParams.probesOriginWorldLocation = params.probesOriginWorldLocation;
	gpuParams.probesEndWorldLocation    = params.probesOriginWorldLocation + volumeSize;
	gpuParams.probesSpacing             = params.probesSpacing;
	gpuParams.rcpProbesSpacing          = gpuParams.probesSpacing.cwiseInverse();
	gpuParams.probesVolumeResolution    = probesVolumeRes;
	gpuParams.probesWrapCoords          = math::Vector3i::Zero();

	gpuParams.probeIlluminanceDataRes           = params.probeIlluminanceDataRes;
	gpuParams.probeIlluminanceDataWithBorderRes = gpuParams.probeIlluminanceDataRes + math::Vector2u::Constant(2u);

	gpuParams.probeHitDistanceDataRes           = params.probeHitDistanceDataRes;
	gpuParams.probeHitDistanceDataWithBorderRes = gpuParams.probeHitDistanceDataRes + math::Vector2u::Constant(2u);

	gpuParams.probesIlluminanceTextureRes = math::Vector2u(probesTextureWidth, probesTextureHeight).cwiseProduct(gpuParams.probeIlluminanceDataWithBorderRes);
	gpuParams.probesHitDistanceTextureRes = math::Vector2u(probesTextureWidth, probesTextureHeight).cwiseProduct(gpuParams.probeHitDistanceDataWithBorderRes);

	gpuParams.probesIlluminanceTexturePixelSize          = gpuParams.probesIlluminanceTextureRes.cast<Real32>().cwiseInverse();
	gpuParams.probesIlluminanceTextureUVDeltaPerProbe    = gpuParams.probesIlluminanceTexturePixelSize.cwiseProduct(gpuParams.probeIlluminanceDataWithBorderRes.cast<Real32>());
	gpuParams.probesIlluminanceTextureUVPerProbeNoBorder = gpuParams.probesIlluminanceTexturePixelSize.cwiseProduct(gpuParams.probeIlluminanceDataRes.cast<Real32>());

	gpuParams.probesHitDistanceTexturePixelSize          = gpuParams.probesHitDistanceTextureRes.cast<Real32>().cwiseInverse();
	gpuParams.probesHitDistanceUVDeltaPerProbe           = gpuParams.probesHitDistanceTexturePixelSize.cwiseProduct(gpuParams.probeHitDistanceDataWithBorderRes.cast<Real32>());
	gpuParams.probesHitDistanceTextureUVPerProbeNoBorder = gpuParams.probesHitDistanceTexturePixelSize.cwiseProduct(gpuParams.probeHitDistanceDataRes.cast<Real32>());
	
	gpuParams.probeIlluminanceEncodingGamma = params.probeIlluminanceEncodingGamma;

	gpuParams.isValid = true;

	const auto createDDGITextureView = [probesTextureWidth, probesTextureHeight](const rdr::RendererResourceName& name, math::Vector2u probeDataRes, rhi::EFragmentFormat format)
	{
		SPT_PROFILER_SCOPE("CreateDDGITextureView");

		rhi::TextureDefinition textureDef;
		textureDef.resolution = math::Vector2u(probesTextureWidth * probeDataRes.x(), probesTextureHeight * probeDataRes.y());
		textureDef.usage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
		textureDef.format = format;
#if RENDERER_VALIDATION
		lib::AddFlag(textureDef.usage, rhi::ETextureUsage::TransferSource);
#endif // RENDERER_VALIDATION
		return rdr::ResourcesManager::CreateTextureView(name, textureDef, rhi::EMemoryUsage::GPUOnly);
	};


	rhi::TextureDefinition probesAverageLuminanceTextureDef;
	probesAverageLuminanceTextureDef.resolution = probesVolumeRes;
	probesAverageLuminanceTextureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferDest);
	probesAverageLuminanceTextureDef.format     = rhi::EFragmentFormat::RGBA16_S_Float;
#if RENDERER_VALIDATION
	lib::AddFlag(probesAverageLuminanceTextureDef.usage, rhi::ETextureUsage::TransferSource);
#endif // RENDERER_VALIDATION
	const lib::SharedRef<rdr::TextureView> probeAverageLuminanceTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("DDGI Volume Probes Average Luminance"), probesAverageLuminanceTextureDef, rhi::EMemoryUsage::GPUOnly);

	const gfx::TexturesBindingsAllocationHandle illuminanceTexturesBlockHandle = m_ddgiSceneDS->u_probesTextures2D.AllocateTexturesBlock(probeDataTexturesNum);
	SPT_CHECK(illuminanceTexturesBlockHandle.IsValid());

	for (Uint32 textureIdx = 0u; textureIdx < probeDataTexturesNum; ++textureIdx)
	{
		const lib::SharedRef<rdr::TextureView> illuminanceTexture = createDDGITextureView(RENDERER_RESOURCE_NAME_FORMATTED("DDGI Volume Probes Illuminance {}", textureIdx), gpuParams.probeIlluminanceDataWithBorderRes, rhi::EFragmentFormat::B10G11R11_U_Float);
		m_ddgiSceneDS->u_probesTextures2D.BindTexture(illuminanceTexture, illuminanceTexturesBlockHandle, textureIdx);
	}

	const gfx::TexturesBindingsAllocationHandle hitDistanceTexturesBlockHandle = m_ddgiSceneDS->u_probesTextures2D.AllocateTexturesBlock(probeDataTexturesNum);
	SPT_CHECK(hitDistanceTexturesBlockHandle.IsValid());

	for (Uint32 textureIdx = 0u; textureIdx < probeDataTexturesNum; ++textureIdx)
	{
		const lib::SharedRef<rdr::TextureView> hitDistanceTexture = createDDGITextureView(RENDERER_RESOURCE_NAME_FORMATTED("DDGI Volume Probes Visibility {}", textureIdx), gpuParams.probeHitDistanceDataWithBorderRes, rhi::EFragmentFormat::RG16_S_Float);
		m_ddgiSceneDS->u_probesTextures2D.BindTexture(hitDistanceTexture, hitDistanceTexturesBlockHandle, textureIdx);
	}

	const Uint32 probeAverageLuminanceTextureIdx = m_ddgiSceneDS->u_probesTextures3D.BindTexture(probeAverageLuminanceTexture);
	SPT_CHECK(probeAverageLuminanceTextureIdx != idxNone<Uint32>);

	gpuParams.illuminanceTextureIdx      = illuminanceTexturesBlockHandle.GetOffset();
	gpuParams.hitDistanceTextureIdx      = hitDistanceTexturesBlockHandle.GetOffset();
	gpuParams.averageLuminanceTextureIdx = probeAverageLuminanceTextureIdx;

	DDGIVolumeGPUDefinition volumeDefinition;
	volumeDefinition.gpuParams                     = gpuParams;
	volumeDefinition.illuminanceTexturesAllocation = illuminanceTexturesBlockHandle;
	volumeDefinition.hitDistanceTexturesAllocation = hitDistanceTexturesBlockHandle;

	return volumeDefinition;
}

void DDGIScene::InitializeLODs(const DDGIConfig& config)
{
	SPT_PROFILER_FUNCTION();

	Uint32 lodsNum = static_cast<Uint32>(config.lodsConfigs.size());

	if (lodsNum > constants::maxLODLevels)
	{
		SPT_LOG_ERROR(DDGIScene, "Too many LODs requested ({}). Max supported number of LODs is {}", lodsNum, constants::maxLODLevels);
		lodsNum = constants::maxLODLevels;
	}

	m_ddgiLODsDef.lodsNum = lodsNum;

	m_lods.resize(lodsNum);

	for (Uint32 lod = 0u; lod < lodsNum; ++lod)
	{
		m_lods[lod].Initialize(*this, config, lod, m_ddgiLODsDef.lods[lod]);
	}
}

void DDGIScene::DeinitializeLODs()
{
	SPT_PROFILER_FUNCTION();

	for (DDGILOD& lod : m_lods)
	{
		lod.Deinitialize(*this);
	}

	m_lods.clear();
}

void DDGIScene::UpdateLODs(const SceneView& mainView)
{
	SPT_PROFILER_FUNCTION();

	for (DDGILOD& lod : m_lods)
	{
		lod.Update(*this, mainView);
	}
}

void DDGIScene::UpdatePriorities(const SceneView& mainView)
{
	SPT_PROFILER_FUNCTION();

	const engn::FrameContext& currentFrame = m_owningScene.GetCurrentFrameRef();

	const Real32 currentTime = currentFrame.GetTime();
	const Real32 deltaTime   = currentFrame.GetDeltaTime();

	for (DDGIVolume* volume : m_volumes)
	{
		volume->UpdateRelitPriority(mainView, currentTime, deltaTime);
	}
}

void DDGIScene::OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	for (DDGIVolume* volume : m_volumes)
	{
		volume->MarkSunLightDirectionDirty();
	}
}

} // spt::rsc::ddgi
