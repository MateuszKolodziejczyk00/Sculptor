#include "RenderStage.h"


namespace spt::rsc
{

RenderStagesFactory& RenderStagesFactory::Get()
{
	static RenderStagesFactory factory;
	return factory;
}

void RenderStagesFactory::RegisterStage(ERenderStage stage, CreateRenderStageFunc createStageFunc)
{
	const size_t idx = RenderStageToIndex(stage);
	SPT_CHECK_MSG(m_createStageFuncs[idx] == nullptr, "Factory function for stage {} is already registered", static_cast<Uint32>(stage));
	m_createStageFuncs[idx] = createStageFunc;
}

lib::UniquePtr<RenderStageBase> RenderStagesFactory::CreateStage(ERenderStage stage)
{
	const CreateRenderStageFunc createStageFunc = m_createStageFuncs[RenderStageToIndex(stage)];
	return createStageFunc.IsValid() ? createStageFunc() : nullptr;
}

RenderStagesFactory::RenderStagesFactory() = default;

} // spt::rsc
