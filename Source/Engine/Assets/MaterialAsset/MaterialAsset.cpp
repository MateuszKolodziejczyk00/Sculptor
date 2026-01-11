#include "MaterialAsset.h"
#include "AssetsSystem.h"
#include "Containers/DynamicArray.h"
#include "DDC.h"
#include "MaterialInstance/MaterialInstance.h"
#include "Transfers/GPUDeferredCommandsQueueTypes.h"
#include "Transfers/GPUDeferredCommandsQueue.h"


SPT_DEFINE_LOG_CATEGORY(MaterialAsset, true);

namespace spt::as
{

struct MaterialDerivedDataHeader
{
	lib::RuntimeTypeInfo materialType;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("MaterialType", materialType);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// MaterialUploadRequest =========================================================================

//class MaterialUploadRequest : public gfx::GPUDeferredUploadRequest
//{
//public:
//
//	MaterialUploadRequest() = default;
//
//	// Begin GPUDeferredUploadRequest overrides
//	virtual void EnqueueUploads() override;
//	// End GPUDeferredUploadRequest overrides
//	
//	lib::MTHandle<rsc::RenderMaterial> renderMaterial;
//	lib::MTHandle<DDCLoadedData<MaterialDerivedDataHeader>> dd;
//};
//
//void MaterialUploadRequest::EnqueueUploads()
//{
//	SPT_PROFILER_FUNCTION();
//
//	SPT_CHECK(renderMaterial.IsValid());
//	SPT_CHECK(dd.IsValid());
//
//	renderMaterial->Initialize(CreateMaterialDefinitionFromDDCData(*dd));
//}

//////////////////////////////////////////////////////////////////////////////////////////////////
// MaterialAsset =================================================================================

ecs::EntityHandle MaterialAsset::GetMaterialEntity() const
{
	return m_materialInstance->GetMaterialEntity();
}

Bool MaterialAsset::Compile()
{
	const MaterialTypeInfo& materialTypeInfo = GetBlackboard().Get<MaterialTypeInfo>();

	lib::UniquePtr<MaterialInstance> instance = MaterialInstanceTypesRegistry::CreateMaterialInstance(materialTypeInfo.materialType);
	SPT_CHECK(!!instance);

	const lib::DynamicArray<Byte> materialData = instance->Compile(*this);
	if (materialData.empty())
	{
		return false;
	}

	MaterialDerivedDataHeader dataHeader;
	dataHeader.materialType = materialTypeInfo.materialType;

	CreateDerivedData(*this, dataHeader, { materialData });

	return true;
}

void MaterialAsset::OnInitialize()
{
	SPT_PROFILER_FUNCTION();

	lib::MTHandle<DDCLoadedData<MaterialDerivedDataHeader>> compiledData = LoadDerivedData<MaterialDerivedDataHeader>(*this);
	SPT_CHECK(compiledData.IsValid());

	m_materialInstance = MaterialInstanceTypesRegistry::CreateMaterialInstance(compiledData->header.materialType);
	SPT_CHECK(!!m_materialInstance);

	m_materialInstance->Load(*this, compiledData.Get());
}

} // spt::as
