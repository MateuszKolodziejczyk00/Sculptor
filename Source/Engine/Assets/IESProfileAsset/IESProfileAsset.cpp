#include "IESProfileAsset.h"
#include "IESProfileImporter.h"
#include "AssetsSystem.h"
#include "IESProfileCompiler.h"
#include "DDC.h"


namespace spt::as
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// IESProfileDataInitializer =====================================================================

void IESProfileDataInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<IESProfileSourceDefinition>(std::move(m_source));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// IESProfileAsset ===============================================================================

void IESProfileAsset::PostCreate()
{
	Super::PostCreate();

	CompileIESProfileTexture();
}

void IESProfileAsset::PostInitialize()
{
	Super::PostInitialize();
}

void IESProfileAsset::OnAssetDeleted(AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data)
{
	Super::OnAssetDeleted(assetSystem, path, data);

	if (const CompiledIESProfile* compiledData = data.blackboard.Find<CompiledIESProfile>())
	{
		assetSystem.GetDDC().DeleteDerivedData(compiledData->derivedDataKey);
	}
}

void IESProfileAsset::CompileIESProfileTexture()
{
	const IESProfileSourceDefinition& sourceDef = GetBlackboard().Get<IESProfileSourceDefinition>();

	const lib::Path profileSourcePath = (GetOwningSystem().GetContentPath() / sourceDef.path);

	const lib::String iesFileContent = lib::File::ReadDocument(profileSourcePath);
	if (!iesFileContent.empty())
	{
		const IESProfileDefinition profileDef = ImportIESProfileFromString(iesFileContent);

		const math::Vector2u resolution(64u, 64u);

		IESProfileCompiler compiler;
		compiler.SetResolution(resolution);

		const IESProfileCompilationResult compilationRes = compiler.Compile(profileDef);

		const DDC& ddc = GetOwningSystem().GetDDC();
		
		const DDCResourceHandle ddcHandle = ddc.CreateDerivedData(compilationRes.textureData.size() * sizeof(Uint16));

		lib::Span<Byte> ddcMemory = ddcHandle.GetMutableSpan();
		std::memcpy(ddcMemory.data(), reinterpret_cast<const Byte*>(compilationRes.textureData.data()), ddcMemory.size());

		CompiledIESProfile compiledProfile;
		compiledProfile.resolution         = compilationRes.resolution;
		compiledProfile.lightSourceCandela = compilationRes.lightSourceCandela;
		compiledProfile.derivedDataKey     = ddcHandle.GetKey();
		GetBlackboard().Create<CompiledIESProfile>(std::move(compiledProfile));
	}
}

} // spt::as
