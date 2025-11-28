#include "gtest/gtest.h"
#include "AssetsSystem.h"
#include "IESProfileAsset.h"
#include "Engine.h"
#include "Renderer.h"


namespace spt::as::tests
{

class IESProfileAssetsSystemTests : public testing::Test
{
protected:

	virtual void SetUp() override;
	virtual void TearDown() override;

	AssetsSystem m_assetsSystem;
};

void IESProfileAssetsSystemTests::SetUp()
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

void IESProfileAssetsSystemTests::TearDown()
{
	m_assetsSystem.Shutdown();
}

TEST_F(IESProfileAssetsSystemTests, CreateIESProfile)
{
	const lib::Path assetPath = "IESProfile/CreateIESProfile/IESProfile.sptasset";

	IESProfileDataInitializer textureInitializer
	{
		IESProfileSourceDefinition{ .path = "IESProfile/CreateIESProfile/Source/sample0.ies" }
	};

	CreateResult result = m_assetsSystem.CreateAsset(AssetInitializer
													 {
														 .type            = CreateAssetType<IESProfileAsset>(),
														 .path            = assetPath,
														 .dataInitializer = &textureInitializer
													 });

	EXPECT_TRUE(result);

	result.GetValue().Reset();

	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 0u);

	LoadResult loadResult = m_assetsSystem.LoadAsset(assetPath);

	EXPECT_TRUE(loadResult);
	loadResult.GetValue().Reset();

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
