#include "StaticMeshPrimitivesSystem.h"
#include "RendererUtils.h"
#include "RenderScene.h"

namespace spt::rsc
{

StaticMeshPrimitivesSystem::StaticMeshPrimitivesSystem(RenderScene& owningScene)
	: Super(owningScene)
{ }

StaticMeshBatch StaticMeshPrimitivesSystem::BuildBatchForView(const RenderView& view) const
{
	SPT_PROFILER_FUNCTION();

	StaticMeshBatch batch;

	const auto meshesView = GetOwningScene().GetRegistry().view<StaticMeshInstanceRenderData, EntityTransformHandle>();
	for (const auto& [entity, staticMeshRenderData, transformHandle] : meshesView.each())
	{
		StaticMeshBatchElement batchElement;
		batchElement.instanceIdx = static_cast<Uint32>(transformHandle.transformSuballocation.GetOffset() / sizeof(math::Affine3f));
		batchElement.staticMeshIdx = staticMeshRenderData.staticMeshIdx;
		batch.batchElements.emplace_back(batchElement);
	}
	SPT_CHECK_NO_ENTRY(); // TODO handle calculating batch counts;

	return batch;
}

} // spt::rsc
