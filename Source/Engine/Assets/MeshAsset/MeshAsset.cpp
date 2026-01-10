#include "MeshAsset.h"
#include "AssetsSystem.h"
#include "DDC.h"
#include "Loaders/GLTFMeshBuilder.h"
#include "Loaders/GLTF.h"
#include "Transfers/GPUDeferredCommandsQueueTypes.h"
#include "StaticMeshes/RenderMesh.h"
#include "Transfers/GPUDeferredCommandsQueue.h"


SPT_DEFINE_LOG_CATEGORY(MeshAsset, true);

namespace spt::as
{

struct MeshDerivedDataHeader
{
	Uint32 meshletsOffset  = 0u;
	Uint32 geometryOffset  = 0u;

	math::Vector3f  boundingSphereCenter = {};
	Real32          boundingSphereRadius = 0.f;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("MeshletsOffset", meshletsOffset);
		serializer.Serialize("GeometryOffset", geometryOffset);

		serializer.Serialize("BoundingSphereCenter", boundingSphereCenter);
		serializer.Serialize("BoundingSphereRadius", boundingSphereRadius);
	}
};


rsc::MeshDefinition CreateMeshDefinitionFromDDCData(const DDCLoadedData<MeshDerivedDataHeader>& dd)
{
	rsc::MeshDefinition meshDef;
	meshDef.meshletsOffset       = dd.header.meshletsOffset;
	meshDef.geometryOffset       = dd.header.geometryOffset;
	meshDef.boundingSphereCenter = dd.header.boundingSphereCenter;
	meshDef.boundingSphereRadius = dd.header.boundingSphereRadius;
	meshDef.blobView             = lib::Span<const Byte>(dd.bin.data(), dd.bin.size());
	return meshDef;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// MeshUploadRequest =============================================================================

class MeshUploadRequest : public gfx::GPUDeferredUploadRequest
{
public:

	MeshUploadRequest() = default;

	// Begin GPUDeferredUploadRequest overrides
	virtual void EnqueueUploads() override;
	// End GPUDeferredUploadRequest overrides
	
	lib::MTHandle<rsc::RenderMesh> renderMesh;
	lib::MTHandle<DDCLoadedData<MeshDerivedDataHeader>> dd;
};

void MeshUploadRequest::EnqueueUploads()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(renderMesh.IsValid());
	SPT_CHECK(dd.IsValid());

	renderMesh->Initialize(CreateMeshDefinitionFromDDCData(*dd));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// MeshBLASBuildRequest ==========================================================================

class MeshBLASBuildRequest : public gfx::GPUDeferredBLASBuildRequest
{
public:

	MeshBLASBuildRequest() = default;

	// Begin GPUDeferredBLASBuildRequest overrides
	virtual void BuildBLASes(rdr::BLASBuilder& builder) override;
	// End GPUDeferredBLASBuildRequest overrides

	lib::MTHandle<rsc::RenderMesh> renderMesh;
};

void MeshBLASBuildRequest::BuildBLASes(rdr::BLASBuilder& builder)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(renderMesh.IsValid());

	renderMesh->BuildBLASes(builder);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// MeshDataInitializer ===========================================================================

void MeshDataInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<MeshSourceDefinition>(std::move(m_source));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// MeshAsset =====================================================================================

Bool MeshAsset::Compile()
{
	const MeshSourceDefinition& sourceDef = GetBlackboard().Get<MeshSourceDefinition>();
	const lib::Path meshSourcePath = (GetDirectoryPath() / sourceDef.path);

	const std::optional<rsc::GLTFModel> gltfModel = rsc::LoadGLTFModel(meshSourcePath.generic_string());
	if (!gltfModel.has_value())
	{
		SPT_LOG_ERROR(MeshAsset, "Failed to load GLTF model from path '{}'", meshSourcePath.generic_string());
		return false;
	}

	rsc::MeshBuildParameters meshBuildParams{};
	rsc::GLTFMeshBuilder meshBuilder(meshBuildParams);
	meshBuilder.BuildMesh(gltfModel->meshes[sourceDef.meshIdx], *gltfModel);
	meshBuilder.Build();

	rsc::MeshDefinition meshDef = meshBuilder.CreateMeshDefinition();

	MeshDerivedDataHeader header;
	header.meshletsOffset = meshDef.meshletsOffset;
	header.geometryOffset = meshDef.geometryOffset;

	header.boundingSphereCenter = meshDef.boundingSphereCenter;
	header.boundingSphereRadius = meshDef.boundingSphereRadius;

	CreateDerivedData(*this, header, { meshDef.blob });

	return true;
}

void MeshAsset::OnInitialize()
{
	SPT_PROFILER_FUNCTION();

	Super::OnInitialize();

	m_renderMesh = new rsc::RenderMesh();
	
	gfx::GPUDeferredCommandsQueue& queue = gfx::GPUDeferredCommandsQueue::Get();

	lib::UniquePtr<MeshUploadRequest> uploadRequest = lib::MakeUnique<MeshUploadRequest>();
	uploadRequest->renderMesh = m_renderMesh;
	uploadRequest->dd = LoadDerivedData<MeshDerivedDataHeader>(*this);
	queue.RequestUpload(std::move(uploadRequest));

	if (rdr::Renderer::IsRayTracingEnabled())
	{
		lib::UniquePtr<MeshBLASBuildRequest> blasBuildRequest = lib::MakeUnique<MeshBLASBuildRequest>();
		blasBuildRequest->renderMesh = m_renderMesh;
		queue.RequestBLASBuild(std::move(blasBuildRequest));
	}
}

} // spt::as
