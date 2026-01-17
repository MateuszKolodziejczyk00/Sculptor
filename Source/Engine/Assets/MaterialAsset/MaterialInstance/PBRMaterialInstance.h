#pragma once

#include "MaterialInstance.h"
#include "AssetTypes.h"
#include "Serialization.h"
#include "Materials/MaterialsRenderingCommon.h"


namespace spt::as
{

struct PBRMaterialDefinition
{
	lib::Path baseColorTexPath;
	lib::Path metallicRoughnessTexPath;
	lib::Path normalsTexPath;
	lib::Path emissiveTexPath;

	math::Vector3f baseColorFactor = math::Vector3f::Ones();
	Real32 metallicFactor          = 0.f;
	Real32 roughnessFactor         = 1.f;
	math::Vector3f emissionFactor  = math::Vector3f::Zero();

	Bool doubleSided   = true;
	Bool customOpacity = false;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("BaseColorTexPath",         baseColorTexPath);
		serializer.Serialize("MetallicRoughnessTexPath", metallicRoughnessTexPath);
		serializer.Serialize("NormalsTexPath",           normalsTexPath);
		serializer.Serialize("EmissiveTexPath",          emissiveTexPath);

		serializer.Serialize("BaseColorFactor", baseColorFactor);
		serializer.Serialize("MetallicFactor",  metallicFactor);
		serializer.Serialize("RoughnessFactor", roughnessFactor);
		serializer.Serialize("EmissionFactor",  emissionFactor);

		serializer.Serialize("DoubleSided",   doubleSided);
		serializer.Serialize("CustomOpacity", customOpacity);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(PBRMaterialDefinition);


class MATERIAL_ASSET_API PBRMaterialInitializer : public AssetDataInitializer
{
public:

	explicit PBRMaterialInitializer(const PBRMaterialDefinition& materialDef)
		: m_definition(materialDef)
	{ }

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	PBRMaterialDefinition m_definition;
};


struct PBRGLTFMaterialDefinition
{
	lib::Path gltfSourcePath;

	// If materialName is valid, it will be used instead of idx
	lib::String materialName = {};
	Uint32      materialIdx  = idxNone<Uint32>;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("GLTFSourcePath", gltfSourcePath);
		serializer.Serialize("MaterialName",   materialName);
		serializer.Serialize("MaterialIdx",    materialIdx);
	}
};
SPT_REGISTER_ASSET_DATA_TYPE(PBRGLTFMaterialDefinition);


class MATERIAL_ASSET_API PBRGLTFMaterialInitializer : public AssetDataInitializer
{
public:

	explicit PBRGLTFMaterialInitializer(const PBRGLTFMaterialDefinition& materialDef)
		: m_definition(materialDef)
	{ }

	virtual void InitializeNewAsset(AssetInstance& asset) override;

private:

	PBRGLTFMaterialDefinition m_definition;
};


class MATERIAL_ASSET_API PBRMaterialInstance : public MaterialInstance
{
public:

	PBRMaterialInstance() = default;
	virtual ~PBRMaterialInstance() override = default;

	virtual lib::DynamicArray<Byte> Compile(const AssetInstance& asset) override;
	virtual void Load(const AssetInstance& asset, lib::MTHandle<DDCLoadedBin> loadedBlob) override;

private:

	rsc::MaterialPBRData m_materialData{};
};

SPT_REGISTER_MATERIAL_INSTANCE_TYPE(PBRMaterialInstance);

} // spt::as
