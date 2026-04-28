#include "gtest/gtest.h"
#include "AssetsSystem.h"
#include "TerrainAsset.h"
#include "Engine.h"
#include "GPUApi.h"
#include "Transfers/GPUDeferredCommandsQueue.h"
#include "MaterialInstance/PBRMaterialInstance.h"


namespace spt::as::tests
{

class TerrainAssetTests : public testing::Test
{
protected:

	virtual void SetUp() override;
	virtual void TearDown() override;

	AssetsSystem m_assetsSystem;
};

void TerrainAssetTests::SetUp()
{
	const lib::Path executablePath = platf::Platform::GetExecutablePath();
	const lib::Path contentPath    = executablePath.parent_path() / "../../Tests/Content";
	const lib::Path ddcPath        = executablePath.parent_path() / "../../Tests/DDC";

	AssetsSystemInitializer initializer;
	initializer.contentPath = contentPath;
	initializer.ddcPath     = ddcPath;
	const Bool initialized = m_assetsSystem.Initialize(initializer);

	EXPECT_TRUE(initialized);
}

void TerrainAssetTests::TearDown()
{
	m_assetsSystem.Shutdown();
}

TEST_F(TerrainAssetTests, CreateTerrainWithoutHeightMap)
{
	const ResourcePath terrainMaterialAssetPath = "Terrain/CreateTerrainWithoutHeightMap/TerrainMaterial.sptasset";
	const ResourcePath terrainAssetPath         = "Terrain/CreateTerrainWithoutHeightMap/Terrain.sptasset";

	m_assetsSystem.DeleteAsset(terrainAssetPath);
	m_assetsSystem.DeleteAsset(terrainMaterialAssetPath);

	TerrainAssetInitializer terrainInitializer
	{
		TerrainAssetDefinition
		{
			.terrainMaterial = "TerrainMaterial.sptasset"
		}
	};

	CreateResult terrainCreateResult = m_assetsSystem.CreateAsset(AssetInitializer
																  {
																	  .type            = CreateAssetType<TerrainAsset>(),
																	  .path            = terrainAssetPath,
																	  .dataInitializer = &terrainInitializer
																  });
	EXPECT_TRUE(terrainCreateResult);

	gfx::GPUDeferredCommandsQueue& queue = engn::GetEngine().GetPluginsManager().GetPluginChecked<gfx::GPUDeferredCommandsQueue>();
	queue.ForceFlushCommands();

	terrainCreateResult.GetValue().Reset();

	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 0u);

	TerrainAssetHandle terrainAsset = m_assetsSystem.LoadAndInitAssetChecked<TerrainAsset>(terrainAssetPath);

	EXPECT_TRUE(terrainAsset.IsValid());
	EXPECT_TRUE(terrainAsset->GetTerrainMaterialAsset().IsValid());
	EXPECT_TRUE(terrainAsset->GetHeightMap() == nullptr);
	EXPECT_TRUE(terrainAsset->GetTerrainDefinition().heightMap == nullptr);

	queue.ForceFlushCommands();

	terrainAsset.Reset();

	queue.ForceFlushCommands();

	EXPECT_TRUE(m_assetsSystem.DeleteAsset(terrainAssetPath) == EDeleteResult::Success);
	EXPECT_TRUE(m_assetsSystem.DeleteAsset(terrainMaterialAssetPath) == EDeleteResult::Success);
}

} // spt::as::tests


int main(int argc, char** argv)
{
	using namespace spt;

	const lib::Path executablePath = platf::Platform::GetExecutablePath();
	const lib::Path enginePath     = executablePath.parent_path() / "../../../";

	const lib::String engineRelativePath    = std::filesystem::relative(enginePath, executablePath).generic_string();
	const lib::String engineRelativePathArg = "-EngineRelativePath=" + engineRelativePath;

	engn::EngineInitializationParams engineInitializationParams;
	engineInitializationParams.additionalArgs.emplace_back(engineRelativePathArg);

	engn::Engine::Initialize(engineInitializationParams);

	rdr::GPUApi::Initialize();

	testing::InitGoogleTest(&argc, argv);

	const auto testsResult = RUN_ALL_TESTS();

	rdr::GPUApi::Uninitialize();

	return testsResult;
}
