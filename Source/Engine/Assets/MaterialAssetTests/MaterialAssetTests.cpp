#include "gtest/gtest.h"
#include "AssetsSystem.h"
#include "MaterialAsset.h"
#include "Engine.h"
#include "MaterialInstance/PBRMaterialInstance.h"
#include "Renderer.h"
#include "Graphics/Transfers/GPUDeferredCommandsQueue.h"
#include "ResourcePath.h"


namespace spt::as::tests
{

class MaterialAssetsTests : public testing::Test
{
protected:

	virtual void SetUp() override;
	virtual void TearDown() override;

	AssetsSystem m_assetsSystem;
};

void MaterialAssetsTests::SetUp()
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

void MaterialAssetsTests::TearDown()
{
	m_assetsSystem.Shutdown();
}

TEST_F(MaterialAssetsTests, CreateMaterial)
{
	const ResourcePath assetPath = "Material/CreateMaterial/Material.sptasset";
	m_assetsSystem.DeleteAsset(assetPath); // Delete leftover asset if exists

	PBRMaterialInitializer materialInitializer
	{
		PBRMaterialDefinition
		{
			.baseColorFactor = math::Vector3f(0.8f, 0.8f, 0.8f),
			.metallicFactor  = 0.f,
			.roughnessFactor = 1.f 
		}
	};

	CreateResult result = m_assetsSystem.CreateAsset(AssetInitializer
													 {
														 .type            = CreateAssetType<MaterialAsset>(),
														 .path            = assetPath,
														 .dataInitializer = &materialInitializer
													 });

	EXPECT_TRUE(result);

	gfx::GPUDeferredCommandsQueue::Get().ForceFlushUploads();

	result.GetValue().Reset();

	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 0u);

	AssetHandle asset = m_assetsSystem.LoadAndInitAssetChecked(assetPath);

	EXPECT_TRUE(asset.IsValid());
	asset.Reset();

	gfx::GPUDeferredCommandsQueue::Get().ForceFlushUploads();

	const EDeleteResult deleteResult = m_assetsSystem.DeleteAsset(assetPath);

	EXPECT_TRUE(deleteResult == EDeleteResult::Success);
}

} // spt::as::tests


int main(int argc, char** argv)
{
	using namespace spt;

	const lib::Path executablePath = platf::Platform::GetExecutablePath();
	const lib::Path enginePath = executablePath.parent_path() / "../../../";

	const lib::String engineRelativePath = std::filesystem::relative(enginePath, executablePath).generic_string();
	const lib::String engineRelativePathArg = "-EngineRelativePath=" + engineRelativePath;

	engn::EngineInitializationParams engineInitializationParams;
	engineInitializationParams.additionalArgs.emplace_back(engineRelativePathArg);

	engn::Engine::Get().Initialize(engineInitializationParams);

	rdr::Renderer::Initialize();

	testing::InitGoogleTest(&argc, argv);

	const auto testsResult = RUN_ALL_TESTS();

	rdr::Renderer::Uninitialize();

	return testsResult;
}
