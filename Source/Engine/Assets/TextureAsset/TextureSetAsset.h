#pragma once

#include "TextureAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "TextureAssetTypes.h"
#include "DDC.h"


namespace spt::rdr
{
class Texture;
} // spt::rdr


namespace spt::as
{


class TEXTURE_ASSET_API TextureSetAsset : public AssetInstance
{
protected:

	using Super = AssetInstance;

public:

	using AssetInstance::AssetInstance;

	virtual void CompileTextureSet() { SPT_CHECK_NO_ENTRY() };
};
SPT_REGISTER_ASSET_TYPE(TextureSetAsset);


struct PBRTextureSetSource
{
	TextureSourceDefinition baseColor;
	TextureSourceDefinition metallicRoughness;
	TextureSourceDefinition normals;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("BaseColor",         baseColor);
		serializer.Serialize("MetallicRoughness", metallicRoughness);
		serializer.Serialize("Normals",           normals);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(PBRTextureSetSource);


struct PBRCompiledTextureSetData
{
	CompiledTexture baseColor;
	CompiledTexture metallicRoughness;
	CompiledTexture normals;

	DerivedDataKey derivedDataKey;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("BaseColor",         baseColor);
		serializer.Serialize("MetallicRoughness", metallicRoughness);
		serializer.Serialize("Normals",           normals);
		serializer.Serialize("DerivedDataKey",    derivedDataKey);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(PBRCompiledTextureSetData);


struct RuntimePBRTextureSetData
{
	lib::SharedPtr<rdr::TextureView> baseColor;
	lib::SharedPtr<rdr::TextureView> metallicRoughness;
	lib::SharedPtr<rdr::TextureView> normals;
};


class TEXTURE_ASSET_API PBRTextureSetInitializer : public AssetDataInitializer
{
public:

	explicit PBRTextureSetInitializer(PBRTextureSetSource source)
		: m_source(std::move(source))
	{
	}

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	PBRTextureSetSource m_source;
};


class TEXTURE_ASSET_API PBRTextureSetAsset : public TextureSetAsset
{
protected:

	using Super = TextureSetAsset;

public:

	using TextureSetAsset::TextureSetAsset;

	// Begin AssetInstance overrides
	virtual void PostCreate() override;
	virtual void PostInitialize() override;

	static void  OnAssetDeleted(AssetsSystem& assetSystem, const ResourcePath& path, const AssetInstanceData& data);
	// End AssetInstance overrides

	// Begin TextureSetAsset overrides
	virtual void CompileTextureSet() override;
	// End TextureSetAsset overrides

	const lib::SharedPtr<rdr::TextureView>& GetBaseColorTextureView()         const { return m_cachedRuntimeTexture->baseColor; }
	const lib::SharedPtr<rdr::TextureView>& GetMetallicRoughnessTextureView() const { return m_cachedRuntimeTexture->metallicRoughness; }
	const lib::SharedPtr<rdr::TextureView>& GetNormalsTextureView()           const { return m_cachedRuntimeTexture->normals; }

private:

	void RequestTextureGPUUploads();

	void CreateTextureInstances();

	RuntimePBRTextureSetData* m_cachedRuntimeTexture = nullptr;
};
SPT_REGISTER_ASSET_TYPE(PBRTextureSetAsset);

} // spt::as
