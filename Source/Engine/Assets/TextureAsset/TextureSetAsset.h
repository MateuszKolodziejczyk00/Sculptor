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


struct CompiledPBRTextureSet
{
	CompiledTexture baseColor;
	CompiledTexture metallicRoughness;
	CompiledTexture normals;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("BaseColor",         baseColor);
		serializer.Serialize("MetallicRoughness", metallicRoughness);
		serializer.Serialize("Normals",           normals);
	}
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


class TEXTURE_ASSET_API PBRTextureSetAsset : public AssetInstance
{
	ASSET_TYPE_GENERATED_BODY(PBRTextureSetAsset, AssetInstance)

public:

	using AssetInstance::AssetInstance;

	const lib::SharedPtr<rdr::TextureView>& GetBaseColorTextureView()         const { return m_baseColor; }
	const lib::SharedPtr<rdr::TextureView>& GetMetallicRoughnessTextureView() const { return m_metallicRoughness; }
	const lib::SharedPtr<rdr::TextureView>& GetNormalsTextureView()           const { return m_normals; }

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void PostInitialize() override;
	// End AssetInstance overrides

private:

	Bool CompileTextureSet();

	void CreateTextureInstances();

	lib::SharedPtr<rdr::TextureView> m_baseColor;
	lib::SharedPtr<rdr::TextureView> m_metallicRoughness;
	lib::SharedPtr<rdr::TextureView> m_normals;
};
SPT_REGISTER_ASSET_TYPE(PBRTextureSetAsset);

} // spt::as
