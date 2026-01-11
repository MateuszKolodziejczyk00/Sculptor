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

	// factors etc..

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
