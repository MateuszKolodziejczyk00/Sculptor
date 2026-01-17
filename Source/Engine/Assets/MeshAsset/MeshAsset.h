#pragma once

#include "Containers/DynamicArray.h"
#include "MeshAssetMacros.h"
#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "DDC.h"


namespace spt::rsc
{
class RenderMesh;
} // spt::rsc


namespace spt::as
{

class MaterialAsset;


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

	Uint32 GetSubmeshesNum() const { return m_submeshesNum; }

protected:

	// Begin AssetInstance overrides
	virtual Bool Compile() override;
	virtual void OnInitialize() override;
	// End AssetInstance overrides

private:

	lib::MTHandle<rsc::RenderMesh> m_renderMesh;
	Uint32 m_submeshesNum = 0u;
};
SPT_REGISTER_ASSET_TYPE(MeshAsset);

} // spt::as
