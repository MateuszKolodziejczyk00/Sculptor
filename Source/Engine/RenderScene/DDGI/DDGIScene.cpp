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

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGILOD0 ======================================================================================

DDGILOD0::DDGILOD0()
{ }

void DDGILOD0::Initialize(DDGIScene& scene, const DDGIConfig& config)
{
	DDGIVolumeParams params;
	params.probesVolumeResolution        = config.lod0VolumeResolution;
	params.probesOriginWorldLocation     = math::Vector3f::Constant(0.f);
	params.probesSpacing                 = config.lod0ProbesSpacing;
	params.probeIlluminanceDataRes       = config.probeIlluminanceDataRes;
	params.probeHitDistanceDataRes       = config.probeHitDistanceDataRes;
	params.probeIlluminanceEncodingGamma = 1.f;
	params.priority                      = 5.f;

	m_volume = scene.BuildVolume(params);

	DDGILOD0Definition lod0Def;
	lod0Def.lod0VolumeIdx = m_volume->GetVolumeIdx();

	const lib::MTHandle<DDGISceneDS>& ddgiDS = scene.GetDDGIDS();
	ddgiDS->u_ddgiLOD0 = lod0Def;
}

void DDGILOD0::Deinitialize(DDGIScene& scene)
{
	m_volume.reset();
}

void DDGILOD0::Update(DDGIScene& scene, const SceneView& mainView)
{
	const math::AlignedBox3f& volumeAABB = m_volume->GetVolumeAABB();
	const math::Vector3f volumeSize = volumeAABB.sizes();

	const math::Vector3f newCenter = mainView.GetLocation() + mainView.GetForwardVector().cwiseProduct(volumeSize) * 0.45f + math::Vector3f(0.0f, 0.0f, volumeSize.x() * 0.25f);

	m_volume->Translate(math::Translation3f(newCenter - volumeAABB.center()));
}

const DDGIVolume& DDGILOD0::GetVolume() const
{
	return *m_volume;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGILOD1 ======================================================================================

DDGILOD1::DDGILOD1()
	: m_singleVolumeSize(math::Vector3f::Zero())
{ }

void DDGILOD1::Initialize(DDGIScene& scene, const DDGIConfig& config)
{
	DDGIVolumeParams params;
	params.probesVolumeResolution        = config.lod1VolumeResolution;
	params.probesOriginWorldLocation     = math::Vector3f::Zero();
	params.probesSpacing                 = config.lod1ProbesSpacing;
	params.probeIlluminanceDataRes       = config.probeIlluminanceDataRes;
	params.probeHitDistanceDataRes       = config.probeHitDistanceDataRes;
	params.probeIlluminanceEncodingGamma = 1.f;
	params.priority                      = 1.f;

	m_singleVolumeSize = params.probesSpacing.cwiseProduct((params.probesVolumeResolution - math::Vector3u::Constant(1)).cast<Real32>());

	const math::Vector3f volumesOrigin = math::Vector3f(-m_singleVolumeSize.x() * 1.5f, -m_singleVolumeSize.y() * 1.5f, config.lod1VolumesMinHeight);

	for (SizeType y = 0; y < 3; ++y)
	{
		for (SizeType x = 0; x < 3; ++x)
		{
			params.probesOriginWorldLocation = volumesOrigin + m_singleVolumeSize.cwiseProduct(math::Vector3f(static_cast<Real32>(x), static_cast<Real32>(y), 0.f));
			m_volumes[x][y] = scene.BuildVolume(params);
		}
	}

	UpdateLODDefinition(scene);
}

void DDGILOD1::Deinitialize(DDGIScene& scene)
{
	for (SizeType y = 0; y < 3; ++y)
	{
		for (SizeType x = 0; x < 3; ++x)
		{
			m_volumes[x][y].reset();
		}
	}

	m_singleVolumeSize = math::Vector3f::Zero();
}

void DDGILOD1::Update(DDGIScene& scene, const SceneView& mainView)
{
	SPT_PROFILER_FUNCTION();

	if (UpdateVolumesLocation(mainView.GetLocation()))
	{
		UpdateLODDefinition(scene);
	}
}

Bool DDGILOD1::UpdateVolumesLocation(const math::Vector3f& desiredCenter)
{
	const lib::SharedPtr<DDGIVolume>& centerVolume = m_volumes[1][1];

	const math::Vector3f offset = (desiredCenter - centerVolume->GetVolumeAABB().min()).cwiseQuotient(m_singleVolumeSize);
	const math::Vector2i gridOffset(std::floor(offset.x()), std::floor(offset.y()));

	if (gridOffset != math::Vector2i::Zero())
	{
		if (std::abs(gridOffset.x()) >= 3 || std::abs(gridOffset.y()) >= 3)
		{
			// Teleport whole grid
			const math::Vector3f newVolumesOrigin = m_volumes[0][0]->GetVolumeAABB().min() + math::Vector3f(gridOffset.x() * m_singleVolumeSize.x(), gridOffset.y() * m_singleVolumeSize.y(), 0.f);

			for (SizeType y = 0; y < 3; ++y)
			{
				for (SizeType x = 0; x < 3; ++x)
				{
					m_volumes[x][y]->TeleportTo(newVolumesOrigin + math::Vector3f(x * m_singleVolumeSize.x(), y * m_singleVolumeSize.y(), 0.f));
				}
			}
		}
		else
		{
			if(gridOffset.y() != 0)
			{
				if (gridOffset.y() > 0)
				{
					for (size_t x = 0; x < m_volumes.size(); ++x)
					{
						const auto rotated = std::rotate(m_volumes[x].begin(), m_volumes[x].begin() + 1, m_volumes[x].end());
						(*rotated)->Translate(math::Translation3f(0.f, 3 * m_singleVolumeSize.y(), 0.f));
					}
				}
				else if (gridOffset.y() < 0)
				{
					for (size_t x = 0; x < m_volumes.size(); ++x)
					{
						const auto rotated = std::rotate(m_volumes[x].rbegin(), m_volumes[x].rbegin() + 1, m_volumes[x].rend());
						(*rotated)->Translate(math::Translation3f(0.f, -3 * m_singleVolumeSize.y(), 0.f));
					}
				}
			}
			else
			{
				const math::Translation3f translation(3 * m_singleVolumeSize.x(), 0.f, 0.f);

				const auto shiftColumns = [translation](auto begin, auto end, int offset)
				{
					const auto rotated = std::rotate(begin, begin + offset, end);
					for (auto current = rotated; current != end; ++current)
					{
						for (lib::SharedPtr<DDGIVolume>& volume : *current)
						{
							volume->Translate(translation);
						}
					}
				};

				if (gridOffset.x() > 0)
				{
					const auto rotated = std::rotate(m_volumes.begin(), m_volumes.begin() + 1, m_volumes.end());
					for (const lib::SharedPtr<DDGIVolume>& volume : *rotated)
					{
						volume->Translate(math::Translation3f(3 * m_singleVolumeSize.x(), 0.f, 0.f));
					}
				}
				else if (gridOffset.x() < 0)
				{
					const auto rotated = std::rotate(m_volumes.rbegin(), m_volumes.rbegin() + 1, m_volumes.rend());
					for (const lib::SharedPtr<DDGIVolume>& volume : *rotated)
					{
						volume->Translate(math::Translation3f(-3 * m_singleVolumeSize.x(), 0.f, 0.f));
					}
				}
			}
		}

		return true;
	}

	return false;
}

void DDGILOD1::UpdateLODDefinition(DDGIScene& scene)
{
	DDGILOD1Definition lod1Def;
	lod1Def.volumesAABBMin      = m_volumes[0][0]->GetVolumeAABB().min();
	lod1Def.volumesAABBMax      = m_volumes[2][2]->GetVolumeAABB().max();
	lod1Def.rcpSingleVolumeSize = m_singleVolumeSize.cwiseInverse();

	Uint32 volumeIdx = 0;
	for (SizeType y = 0; y < 3; ++y)
	{
		for (SizeType x = 0; x < 3; ++x)
		{
			lod1Def.volumeIndices[volumeIdx++].x() = m_volumes[x][y]->GetVolumeIdx();
		}
	}

	scene.GetDDGIDS()->u_ddgiLOD1 = lod1Def;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIScene =====================================================================================

DDGIScene::DDGIScene(RenderScene& renderScene)
	: m_owningScene(renderScene)
	, m_ddgiSceneDS(rdr::ResourcesManager::CreateDescriptorSetState<DDGISceneDS>(RENDERER_RESOURCE_NAME("DDGI Scene DS"), rdr::EDescriptorSetStateFlags::Persistent))
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

	FlushDataChanges();

	UpdatePriorities(mainView);

	SortVolumes();
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

const DDGILOD0& DDGIScene::GetLOD0() const
{
	return m_lod0;
}

const DDGILOD1& DDGIScene::GetLOD1() const
{
	return m_lod1;
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

	volume->Initialize(gpuVolumeHandle, params);

	return volume;
}

Uint32 DDGIScene::FindAvailableVolumeIdx() const
{
	const SizeType volumesCount = m_ddgiSceneDS->u_ddgiVolumes.GetSize();

	for (SizeType i = 0; i < volumesCount; ++i)
	{
		if (m_ddgiSceneDS->u_ddgiVolumes[i].isValid == false)
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

	DDGIGPUVolumeHandle newVolumeHandle(m_ddgiSceneDS, volumeIdx);

	newVolumeHandle.GetGPUParamsMutable() = CreateGPUData(params);

	return newVolumeHandle;
}

DDGIVolumeGPUParams DDGIScene::CreateGPUData(const DDGIVolumeParams& params) const
{
	const math::Vector3u probesVolumeRes = params.probesVolumeResolution;

	const Uint32 probesTextureWidth  = probesVolumeRes.x() * probesVolumeRes.z();
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


	const lib::SharedRef<rdr::TextureView> illuminanceTexture = createDDGITextureView(RENDERER_RESOURCE_NAME("DDGI Volume Probes Illuminance"), gpuParams.probeIlluminanceDataWithBorderRes, rhi::EFragmentFormat::B10G11R11_U_Float);
	const lib::SharedRef<rdr::TextureView> hitDistanceTexture = createDDGITextureView(RENDERER_RESOURCE_NAME("DDGI Volume Probes Visibility"), gpuParams.probeHitDistanceDataWithBorderRes, rhi::EFragmentFormat::RG16_S_Float);;

	const Uint32 illuminanceTextureIdx = m_ddgiSceneDS->u_probesTextures.BindTexture(illuminanceTexture);
	SPT_CHECK(illuminanceTextureIdx != idxNone<Uint32>);

	const Uint32 hitDistanceTextureIdx = m_ddgiSceneDS->u_probesTextures.BindTexture(hitDistanceTexture);
	SPT_CHECK(hitDistanceTextureIdx != idxNone<Uint32>);

	gpuParams.illuminanceTextureIdx = illuminanceTextureIdx;
	gpuParams.hitDistanceTextureIdx = hitDistanceTextureIdx;

	return gpuParams;
}

void DDGIScene::InitializeLODs(const DDGIConfig& config)
{
	SPT_PROFILER_FUNCTION();

	m_lod0.Initialize(*this, config);
	m_lod1.Initialize(*this, config);
}

void DDGIScene::DeinitializeLODs()
{
	SPT_PROFILER_FUNCTION();

	m_lod0.Deinitialize(*this);
	m_lod1.Deinitialize(*this);
}

void DDGIScene::UpdateLODs(const SceneView& mainView)
{
	SPT_PROFILER_FUNCTION();

	m_lod0.Update(*this, mainView);
	m_lod1.Update(*this, mainView);
}

void DDGIScene::FlushDataChanges()
{
	SPT_PROFILER_FUNCTION();

	m_ddgiSceneDS->u_ddgiVolumes.Flush();
}

void DDGIScene::UpdatePriorities(const SceneView& mainView)
{
	SPT_PROFILER_FUNCTION();

	const Real32 deltaTime = engn::GetRenderingFrame().GetDeltaTime();

	for (DDGIVolume* volume : m_volumes)
	{
		volume->UpdateRelitPriority(mainView, deltaTime);
	}
}

void DDGIScene::SortVolumes()
{
	SPT_PROFILER_FUNCTION();

	std::sort(std::begin(m_volumes), std::end(m_volumes),
			  [](const DDGIVolume* lhs, const DDGIVolume* rhs)
			  {
				  return lhs->GetRelitPriority() > rhs->GetRelitPriority();
			  });
}

void DDGIScene::OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity)
{
	for (DDGIVolume* volume : m_volumes)
	{
		volume->RequestGlobalRelit();
	}
}

} // spt::rsc::ddgi
