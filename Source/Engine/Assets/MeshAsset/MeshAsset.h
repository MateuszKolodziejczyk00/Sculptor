#pragma once

#include "MeshAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "DDC.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{
class RenderMesh;
} // spt::rsc


namespace spt::as
{

struct MeshSourceDefinition
{
	lib::Path path;

	// Meshes in source file are accessed either by name or idx
	Uint32      meshIdx = idxNone<Uint32>;
	lib::String meshName;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Path",     path);
		serializer.Serialize("MeshIdx",  meshIdx);
		serializer.Serialize("MeshName", meshName);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(MeshSourceDefinition);


class MESH_ASSET_API MeshDataInitializer : public AssetDataInitializer
{
public:

	explicit MeshDataInitializer(const MeshSourceDefinition& source)
		: m_source(source)
	{
	}

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	MeshSourceDefinition m_source;
};


class MESH_ASSET_API MeshAsset : public AssetInstance
{
	ASSET_TYPE_GENERATED_BODY(MeshAsset, AssetInstance)

public:

	using AssetInstance::AssetInstance;

	const lib::MTHandle<rsc::RenderMesh>& GetRenderMesh() const { return m_renderMesh; }

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void PostInitialize() override;
	// End AssetInstance overrides

private:

	lib::MTHandle<rsc::RenderMesh> m_renderMesh;
};
SPT_REGISTER_ASSET_TYPE(MeshAsset);

} // spt::as
