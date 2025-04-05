#include "gtest/gtest.h"
#include "AssetsSystem.h"
#include "EngineCore/Paths.h"

namespace spt::as::tests
{

struct AssetData1
{
	Uint32 value = 0u;
};

struct AssetData2
{
	Uint32 value1 = 0u;
	Uint32 value2 = 0u;
};

} // spt::as::tests

SPT_REGISTER_ASSET_DATA_TYPE(spt::as::tests::AssetData1);
SPT_REGISTER_ASSET_DATA_TYPE(spt::as::tests::AssetData2);

namespace spt::srl
{

template<>
struct TypeSerializer<as::tests::AssetData1>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("Value", data.value);
	}
};

template<>
struct TypeSerializer<as::tests::AssetData2>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("Value1", data.value1);
		serializer.Serialize("Value2", data.value2);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::as::tests::AssetData1);
SPT_YAML_SERIALIZATION_TEMPLATES(spt::as::tests::AssetData2);

namespace spt::as::tests
{

class AssetsSystemTests : public testing::Test
{
protected:

	virtual void SetUp() override;
	virtual void TearDown() override;

	AssetsSystem m_assetsSystem;
};

void AssetsSystemTests::SetUp()
{
	const lib::Path executablePath = platf::Platform::GetExecutablePath();
	const lib::Path contentPath = executablePath.parent_path() / "../../Content/TestContent";

	AssetsSystemInitializer initializer;
	initializer.contentPath = contentPath;
	const Bool initialized = m_assetsSystem.Initialize(initializer);

	EXPECT_TRUE(initialized);
}

void AssetsSystemTests::TearDown()
{
	m_assetsSystem.Shutdown();
}

TEST(AssetsSystemInitializationTests, Initialization)
{
	AssetsSystem assetsSystem;

	const lib::Path executablePath = platf::Platform::GetExecutablePath();
	const lib::Path contentPath = executablePath.parent_path() / "../../Content/TestContent";

	AssetsSystemInitializer initializer;
	initializer.contentPath = contentPath;
	const Bool initialized = assetsSystem.Initialize(initializer);

	EXPECT_TRUE(initialized);

	assetsSystem.Shutdown();
}

TEST_F(AssetsSystemTests, CreateAndDeleteAsset)
{
	lib::Path assetPath = "CreateAndDeleteAsset/Asset.sptasset";

	CreateResult result = m_assetsSystem.CreateAsset(AssetInitializer
													 {
														 .path = assetPath,
													 });

	EXPECT_TRUE(result.HasValue());
	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 1u);

	EXPECT_TRUE(m_assetsSystem.DoesAssetExist(assetPath));

	result.GetValue().Reset();

	const EDeleteResult deleteResult = m_assetsSystem.DeleteAsset(assetPath);

	EXPECT_TRUE(deleteResult == EDeleteResult::Success);

	EXPECT_FALSE(m_assetsSystem.DoesAssetExist(assetPath));
	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 0u);
}

TEST_F(AssetsSystemTests, LoadAsset)
{
	lib::Path assetPath = "LoadAsset/Asset.sptasset";

	EXPECT_TRUE(m_assetsSystem.DoesAssetExist(assetPath));

	const LoadResult loadResult = m_assetsSystem.LoadAsset(assetPath);

	EXPECT_TRUE(loadResult.HasValue());

	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 1u);
}

TEST_F(AssetsSystemTests, AssetDataUnload)
{
	const ResourcePath assetPath = "AssetDataUnload/Asset.sptasset";

	AssetHandle asset = m_assetsSystem.LoadAssetChecked(assetPath);

	asset->GetBlackboard().Unload(lib::TypeInfo<AssetData1>());

	EXPECT_TRUE(asset->GetBlackboard().Contains<AssetData1>() == false);
	EXPECT_TRUE(asset->GetBlackboard().GetUnloadedTypes().size() == 1u);

	asset->SaveAsset();
	asset.Reset();

	asset = m_assetsSystem.LoadAssetChecked(assetPath);

	EXPECT_TRUE(asset->GetBlackboard().Contains<AssetData1>() == true);
}

TEST_F(AssetsSystemTests, AssetsLoadingAndUnloading)
{
	const ResourcePath asset1Path = "AssetsLoadingAndUnloading/Asset1.sptasset";
	const ResourcePath asset2Path = "AssetsLoadingAndUnloading/Asset2.sptasset";

	EXPECT_TRUE(m_assetsSystem.DoesAssetExist(asset1Path));
	EXPECT_TRUE(m_assetsSystem.DoesAssetExist(asset2Path));

	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 0u);

	AssetHandle asset1 = m_assetsSystem.LoadAssetChecked(asset1Path);
	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 1u);

	AssetHandle asset2 = m_assetsSystem.LoadAssetChecked(asset2Path);
	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 2u);

	// Loading already loaded asset
	AssetHandle asset1Again = m_assetsSystem.LoadAssetChecked(asset1Path);
	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 2u);

	// Create already loaded asset
	CreateResult createRes1 = m_assetsSystem.CreateAsset(AssetInitializer
														 {
															 .path = asset1Path,
														 });

	EXPECT_TRUE(createRes1.HasError());

	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 2u);

	asset1.Reset();
	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 2u);

	asset1Again.Reset();
	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 1u);

	// Load unloaded asset
	asset1 = m_assetsSystem.LoadAssetChecked(asset1Path);
	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 2u);

	asset1.Reset();
	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 1u);

	asset2.Reset();
	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 0u);
}


TEST_F(AssetsSystemTests, CreateAndLoadAssetWithData)
{
	const lib::Path assetPath = "CreateAndLoadAssetWithData/Asset.sptasset";

	EXPECT_TRUE(!m_assetsSystem.DoesAssetExist(assetPath));

	CreateResult createResult = m_assetsSystem.CreateAsset(AssetInitializer
														   {
															   .path = assetPath,
														   });

	EXPECT_TRUE(createResult.HasValue());

	AssetHandle asset = std::move(createResult.GetValue());

	const Uint32 val = 42u;

	asset->GetBlackboard().Create<AssetData1>(AssetData1{ .value = val });
	asset->GetBlackboard().Create<AssetData2>(AssetData2{ .value2 = val });
	asset->SaveAsset();

	asset.Reset();

	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 0u);

	LoadResult loadResult = m_assetsSystem.LoadAsset(assetPath);

	EXPECT_TRUE(loadResult.HasValue());

	asset = std::move(loadResult.GetValue());

	EXPECT_TRUE(asset->GetBlackboard().Contains<AssetData1>());
	EXPECT_TRUE(asset->GetBlackboard().Contains<AssetData2>());

	EXPECT_TRUE(asset->GetBlackboard().Get<AssetData1>().value == val);
	EXPECT_TRUE(asset->GetBlackboard().Get<AssetData2>().value2 == val);
	EXPECT_TRUE(asset->GetBlackboard().Get<AssetData2>().value1 == 0u);

	m_assetsSystem.DeleteAsset(asset->GetPath());
	asset.Reset();

	EXPECT_TRUE(m_assetsSystem.GetLoadedAssetsList().size() == 0u);
	EXPECT_TRUE(!m_assetsSystem.DoesAssetExist(assetPath));
}

} // spt::as::tests


int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);

	using namespace spt;

	const auto testsResult = RUN_ALL_TESTS();

	return testsResult;
}
