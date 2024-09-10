
#pragma once

#include "SculptorCoreTypes.h"
#include "View/ViewRenderingSpec.h"
#include "RHICore/RHITextureTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;


struct RenderTargetFormatsDef
{
	lib::DynamicArray<rhi::EFragmentFormat> colorRTFormats;
	rhi::EFragmentFormat					depthRTFormat;
};


class RenderStageBase
{
public:

	virtual ~RenderStageBase() = default;
	
	virtual void Initialize(RenderView& renderView) {}
	virtual void Deinitialize(RenderView& renderView) {}

	virtual void BeginFrame(const RenderScene& renderScene, ViewRenderingSpec& viewSpec) {}
	virtual void EndFrame(const RenderScene& renderScene, const RenderView& renderView)  {}
};


using CreateRenderStageFunc = lib::RawCallable<lib::UniquePtr<RenderStageBase>()>;


class RenderStagesFactory
{
public:

	static RenderStagesFactory& Get();

	void RegisterStage(ERenderStage stage, CreateRenderStageFunc createStageFunc);

	lib::UniquePtr<RenderStageBase> CreateStage(ERenderStage stage);

private:

	RenderStagesFactory();

	static constexpr size_t s_stagesNum = sizeof(ERenderStage) * 8;

	lib::StaticArray<CreateRenderStageFunc, s_stagesNum> m_createStageFuncs;
};


template<ERenderStage stage, typename TRenderStageType>
struct RenderStageRegistration
{
	RenderStageRegistration()
	{
		RenderStagesFactory::Get().RegisterStage(stage, &RenderStageRegistration::CreateStage);
	}

	static lib::UniquePtr<RenderStageBase> CreateStage()
	{
		return std::make_unique<TRenderStageType>();
	}
};


#define REGISTER_RENDER_STAGE(stage, renderStageType) \
	static RenderStageRegistration<stage, renderStageType> s_renderStageRegistration_##renderStageType;


template<typename TRenderStageType, ERenderStage stage>
class RenderStage : public RenderStageBase
{
public:

	RenderStage() = default;

	static constexpr ERenderStage GetStageEnum() { return stage; }

	void Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SceneRendererSettings& settings);

	RenderStageEntries& GetStageEntries(ViewRenderingSpec& viewSpec);
};


template<typename TRenderStageType, ERenderStage stage>
inline void RenderStage<TRenderStageType, stage>::Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	RenderStageEntries& viewStageEntries = GetStageEntries(viewSpec);

	RenderStageExecutionContext stageContext(settings, stage);

	viewStageEntries.BroadcastPreRenderStage(graphBuilder, renderScene, viewSpec, stageContext);

	reinterpret_cast<TRenderStageType*>(this)->OnRender(graphBuilder, renderScene, viewSpec, stageContext);
	
	viewStageEntries.BroadcastPostRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

template<typename TRenderStageType, ERenderStage stage>
inline RenderStageEntries& RenderStage<TRenderStageType, stage>::GetStageEntries(ViewRenderingSpec& viewSpec)
{
	return viewSpec.GetRenderStageEntries(stage);
}

} // spt::rsc
