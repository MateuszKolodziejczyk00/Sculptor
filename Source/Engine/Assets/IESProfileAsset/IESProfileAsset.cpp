#include "IESProfileAsset.h"
#include "IESProfileImporter.h"
#include "AssetsSystem.h"
#include "IESProfileCompiler.h"
#include "DDC.h"
#include "Transfers/GPUDeferredUploadsQueue.h"
#include "Transfers/GPUDeferredUploadsQueueTypes.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/Texture.h"
#include "Transfers/UploadUtils.h"
#include "ResourcesManager.h"


namespace spt::as
{

struct IESProfileDerivedDataHeader
{
	math::Vector2u textureRes = math::Vector2u(0u, 0u);

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("TextureRes", textureRes);
	}
};


//////////////////////////////////////////////////////////////////////////////////////////////////
// IESProfileUploadRequest =======================================================================

class IESProfileUploadRequest : public gfx::GPUDeferredUploadRequest
{
public:

	IESProfileUploadRequest() = default;

	// Begin GPUDeferredUploadRequest overrides
	virtual void PrepareForUpload(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& preUploadDependency) override;
	virtual void EnqueueUploads() override;
	virtual void FinishStreaming(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& postUploadDependency) override;
	// End GPUDeferredUploadRequest overrides
	
	lib::SharedPtr<rdr::TextureView>                          dstTextureView;
	lib::MTHandle<DDCLoadedData<IESProfileDerivedDataHeader>> dd;
};

void IESProfileUploadRequest::PrepareForUpload(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& preUploadDependency)
{
	const SizeType dependencyIdx = preUploadDependency.AddTextureDependency(dstTextureView->GetTexture()->GetRHI(), dstTextureView->GetRHI().GetSubresourceRange());
	preUploadDependency.SetLayoutTransition(dependencyIdx, rhi::TextureTransition::TransferDest);
}

void IESProfileUploadRequest::EnqueueUploads()
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedRef<rdr::Texture>& texture = dstTextureView->GetTexture();

	const rhi::ETextureAspect textureAspect = dstTextureView->GetRHI().GetAspect();

	gfx::UploadDataToTexture(dd->bin.data(), dd->bin.size(), texture, textureAspect, texture->GetRHI().GetResolution(), math::Vector3u::Zero(), 0u, 0u);
}

void IESProfileUploadRequest::FinishStreaming(rdr::CommandRecorder& cmdRecorder, rhi::RHIDependency& postUploadDependency)
{
	const SizeType dependencyIdx = postUploadDependency.AddTextureDependency(dstTextureView->GetTexture()->GetRHI(), dstTextureView->GetRHI().GetSubresourceRange());
	postUploadDependency.SetLayoutTransition(dependencyIdx, rhi::TextureTransition::ReadOnly);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// IESProfileDataInitializer =====================================================================

void IESProfileDataInitializer::InitializeNewAsset(AssetInstance& asset)
{
	asset.GetBlackboard().Create<IESProfileSourceDefinition>(std::move(m_source));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// IESProfileAsset ===============================================================================

Bool IESProfileAsset::Compile()
{
	return CompileIESProfileTexture();
}

void IESProfileAsset::PostInitialize()
{
	Super::PostInitialize();

	InitGPUTexture();
}

Bool IESProfileAsset::CompileIESProfileTexture()
{
	SPT_PROFILER_FUNCTION();

	const IESProfileSourceDefinition& sourceDef = GetBlackboard().Get<IESProfileSourceDefinition>();

	const lib::Path profileSourcePath = (GetDirectoryPath() / sourceDef.path);

	const lib::String iesFileContent = lib::File::ReadDocument(profileSourcePath);
	if (!iesFileContent.empty())
	{
		const IESProfileDefinition profileDef = ImportIESProfileFromString(iesFileContent);

		const math::Vector2u resolution(64u, 64u);

		IESProfileCompiler compiler;
		compiler.SetResolution(resolution);

		const IESProfileCompilationResult compilationRes = compiler.Compile(profileDef);

		const lib::Span<const Byte> textureDataAsBytes = lib::Span<const Byte>(reinterpret_cast<const Byte*>(compilationRes.textureData.data()), compilationRes.textureData.size() * sizeof(Uint16));

		IESProfileDerivedDataHeader header;
		header.textureRes = resolution;
		CreateDerivedData(*this, header, textureDataAsBytes);

		return true;
	}

	return false;
}

void IESProfileAsset::InitGPUTexture()
{
	SPT_PROFILER_FUNCTION();

	lib::MTHandle<DDCLoadedData<IESProfileDerivedDataHeader>> dd = LoadDerivedData<IESProfileDerivedDataHeader>(*this);

	const rhi::TextureDefinition textureDef(dd->header.textureRes, lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest), rhi::EFragmentFormat::R16_UN_Float);

	m_texture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME_FORMATTED("IESProfileTexture_{}", GetName().ToString()),
														 textureDef,
														 rhi::RHIAllocationInfo(rhi::EMemoryUsage::GPUOnly));

	gfx::GPUDeferredUploadsQueue& queue = gfx::GPUDeferredUploadsQueue::Get();

	lib::UniquePtr<IESProfileUploadRequest> uploadRequest = lib::MakeUnique<IESProfileUploadRequest>();
	uploadRequest->dstTextureView = m_texture;
	uploadRequest->dd             = std::move(dd);
	queue.RequestUpload(std::move(uploadRequest));
}

} // spt::as
