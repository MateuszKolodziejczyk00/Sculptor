
#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "Utility/Templates/Callable.h"
#include "Utils/ViewRenderingSpec.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc
{

class RenderView;
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
	
	void Initialize(RenderView& renderView) {}
	void Deinitialize(RenderView& renderView) {}

	void BeginFrame(const RenderScene& renderScene, ViewRenderingSpec& viewSpec) {}
	void EndFrame(const RenderScene& renderScene, const RenderView& renderView)  {}
};


template<typename TRenderStageType, ERenderStage stage>
class RenderStage : public RenderStageBase
{
public:

	RenderStage() = default;

	static constexpr ERenderStage GetStageEnum() { return stage; }

	void Render(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SceneRendererSettings& settings);
};


template<typename TRenderStageType, ERenderStage stage>
inline void RenderStage<TRenderStageType, stage>::Render(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	RenderStageExecutionContext stageContext(settings, stage);
	reinterpret_cast<TRenderStageType*>(this)->OnRender(graphBuilder, rendererInterface, renderScene, viewSpec, stageContext);
}


class RenderStagesAPI
{
public:

	static RenderStagesAPI& Get();

	RenderStageBase* CallConstructor(ERenderStage stageType, lib::MemoryArena& arena) const;
	void             CallDestructor(ERenderStage stageType, RenderStageBase& stage) const;
	void             CallInitializeFunc(ERenderStage stageType, RenderStageBase& stage, RenderView& renderView) const;
	void             CallDeinitializeFunc(ERenderStage stageType, RenderStageBase& stage, RenderView& renderView) const;
	void             CallBeginFrameFunc(ERenderStage stageType, RenderStageBase& stage, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) const;
	void             CallEndFrameFunc(ERenderStage stageType, RenderStageBase& stage, const RenderScene& renderScene, const RenderView& renderView) const;
	void             CallRenderFunc(ERenderStage stageType, RenderStageBase& stage, rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SceneRendererSettings& settings) const;

	template<ERenderStage stage, typename TRenderStageType>
	void RegisterStage()
	{
		const Uint32 stageIdx = RenderStageToIdx(stage);

		const auto constructor = [](lib::MemoryArena& arena) -> RenderStageBase*
		{
			return arena.AllocateType<TRenderStageType>();
		};

		const auto destructor = [](RenderStageBase& s)
		{
			reinterpret_cast<TRenderStageType&>(s).~TRenderStageType();
		};

		const auto initializeFunc = [](RenderStageBase& s, RenderView& renderView)
		{
			static_cast<TRenderStageType&>(s).Initialize(renderView);
		};

		const auto deinitializeFunc = [](RenderStageBase& s, RenderView& renderView)
		{
			static_cast<TRenderStageType&>(s).Deinitialize(renderView);
		};

		const auto beginFrameFunc = [](RenderStageBase& s, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
		{
			static_cast<TRenderStageType&>(s).BeginFrame(renderScene, viewSpec);
		};

		const auto endFrameFunc = [](RenderStageBase& s, const RenderScene& renderScene, const RenderView& renderView)
		{
			static_cast<TRenderStageType&>(s).EndFrame(renderScene, renderView);
		};

		const auto renderFunc = [](RenderStageBase& s, rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SceneRendererSettings& settings)
		{
			static_cast<TRenderStageType&>(s).Render(graphBuilder, rendererInterface, renderScene, viewSpec, settings);
		};

		m_stageEntries[stageIdx] = RenderStageEntry{
			.constructor      = constructor,
			.destructor       = destructor,
			.initializeFunc   = initializeFunc,
			.deinitializeFunc = deinitializeFunc,
			.beginFrameFunc   = beginFrameFunc,
			.endFrameFunc     = endFrameFunc,
			.renderFunc       = renderFunc
		};
	}

private:

	struct RenderStageEntry
	{
		lib::RawCallable<RenderStageBase*(lib::MemoryArena&)>                                                                                                            constructor;
		lib::RawCallable<void(RenderStageBase&)>                                                                                                                         destructor;
		lib::RawCallable<void(RenderStageBase&, RenderView&)>                                                                                                            initializeFunc;
		lib::RawCallable<void(RenderStageBase&, RenderView&)>                                                                                                            deinitializeFunc;
		lib::RawCallable<void(RenderStageBase&, const RenderScene&, ViewRenderingSpec&)>                                                                                 beginFrameFunc;
		lib::RawCallable<void(RenderStageBase&, const RenderScene&, const RenderView&)>                                                                                  endFrameFunc;
		lib::RawCallable<void(RenderStageBase&, rg::RenderGraphBuilder&, SceneRendererInterface&, const RenderScene&, ViewRenderingSpec&, const SceneRendererSettings&)> renderFunc;
	};

	const RenderStageEntry& GetStageEntry(ERenderStage stage) const;

	lib::StaticArray<RenderStageEntry, static_cast<size_t>(ERenderStage::NUM)> m_stageEntries;
};


template<ERenderStage stage, typename TRenderStageType>
struct RenderStageRegistration
{
	RenderStageRegistration()
	{
		RenderStagesAPI::Get().RegisterStage<stage, TRenderStageType>();
	}
};


#define REGISTER_RENDER_STAGE(stage, renderStageType) \
SCENE_RENDERER_API RenderStageRegistration<stage, renderStageType> s_renderStageRegistration_##renderStageType;

} // spt::rsc
