#include "SceneAsset.h"
#include "AssetsSystem.h"
#include "Generated/RenderSceneSettingsDef.h"


SPT_DEFINE_LOG_CATEGORY(SceneAsset, true);

namespace spt::as
{

struct SceneDDCHeader
{
	ResourcePathID prefab     = InvalidResourcePathID;
	ResourcePathID terrain    = InvalidResourcePathID;
	ResourcePathID cloudscape = InvalidResourcePathID;
	srl::JSON      sceneSettings;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Prefab",        prefab);
		serializer.Serialize("Terrain",       terrain);
		serializer.Serialize("Cloudscape",    cloudscape);
		serializer.Serialize("SceneSettings", sceneSettings);
	}
};


void SceneAsset::ApplySceneSettings(rsc::RenderSceneSettings& settings) const
{
	SPT_CHECK(IsInitialized());

	srl::Serializer reader = srl::Serializer::CreateReader(m_sceneSettings);
	reader.SerializeObject(settings);
}

Bool SceneAsset::Compile()
{
	SPT_PROFILER_FUNCTION();

	const SceneDefinition& definition = GetBlackboard().Get<SceneDefinition>();

	Bool success = true;

	const auto compileReference = [this, &success](const lib::Path& relativePath) -> ResourcePathID
	{
		if (relativePath.empty())
		{
			return InvalidResourcePathID;
		}

		const ResourcePath path = ResolveAssetRelativePath(relativePath);
		if (!GetOwningSystem().CompileAssetIfDeprecated(path))
		{
			SPT_LOG_ERROR(SceneAsset, "Scene '{}' references invalid asset '{}'", GetName().ToString(), relativePath.generic_string());
			success = false;
			return InvalidResourcePathID;
		}

		return path.GetID();
	};

	SceneDDCHeader header;
	header.prefab        = compileReference(definition.prefab);
	header.terrain       = compileReference(definition.terrain);
	header.cloudscape    = compileReference(definition.cloudscape);
	header.sceneSettings = definition.sceneSettings;

	if (success)
	{
		CreateDerivedData(*this, header, lib::Span<const Byte>{});
	}

	return success;
}

void SceneAsset::OnInitialize()
{
	SPT_PROFILER_FUNCTION();

	const lib::MTHandle<DDCLoadedData<SceneDDCHeader>> dd = LoadDerivedData<SceneDDCHeader>(*this);
	SPT_CHECK(dd.IsValid());

	AssetsSystem& assetsSystem = GetOwningSystem();

	if (dd->header.prefab != InvalidResourcePathID)
	{
		m_prefab = assetsSystem.LoadAssetChecked<PrefabAsset>(dd->header.prefab);
	}

	if (dd->header.terrain != InvalidResourcePathID)
	{
		m_terrain = assetsSystem.LoadAssetChecked<TerrainAsset>(dd->header.terrain);
	}

	if (dd->header.cloudscape != InvalidResourcePathID)
	{
		m_cloudscape = assetsSystem.LoadAssetChecked<CloudscapeAsset>(dd->header.cloudscape);
	}

	m_sceneSettings = std::move(dd->header.sceneSettings);

	for (const AssetHandle& asset : { AssetHandle(m_prefab), AssetHandle(m_terrain), AssetHandle(m_cloudscape) })
	{
		if (asset.IsValid())
		{
			asset->AwaitInitialization();
		}
	}
}

} // spt::as
