#include "TemporalAAViewRenderSystem.h"
#include "View/ViewRenderingSpec.h"
#include "Techniques/TemporalAA/TemporalAATypes.h"
#include "RenderGraphBuilder.h"


namespace spt::rsc
{

TemporalAAViewRenderSystem::TemporalAAViewRenderSystem()
{
}

void TemporalAAViewRenderSystem::SetTemporalAARenderer(lib::UniquePtr<gfx::TemporalAARenderer> temporalAARenderer)
{
	const lib::LockGuard lockGuard(m_temporalAARendererLock);

	m_requestedTemporalAARenderer = std::move(temporalAARenderer);
}

lib::StringView TemporalAAViewRenderSystem::GetRendererName() const
{
	const lib::LockGuard lockGuard(m_temporalAARendererLock);

	if (m_temporalAARenderer)
	{
		return m_temporalAARenderer->GetName();
	}

	return "None";
}

void TemporalAAViewRenderSystem::PrepareRenderView(RenderView& renderView)
{
	Super::PrepareRenderView(renderView);

	{
		const lib::LockGuard lockGuard(m_temporalAARendererLock);
		if (m_requestedTemporalAARenderer)
		{
			m_temporalAARenderer = std::move(*m_requestedTemporalAARenderer);
			m_requestedTemporalAARenderer.reset();
		}
	}

	Bool preparedAA = false;

	if (m_temporalAARenderer)
	{
		gfx::TemporalAAParams aaParams;
		aaParams.inputResolution  = renderView.GetRenderingRes();
		aaParams.outputResolution = renderView.GetRenderingRes();

		preparedAA = m_temporalAARenderer->PrepareForRendering(aaParams);
	}

	if (preparedAA)
	{
		const math::Vector2f jitter = m_temporalAARenderer->ComputeJitter(renderView.GetRenderedFrameIdx(), renderView.GetRenderingRes());
		renderView.SetJitter(jitter);
	}
	else
	{
		renderView.ResetJitter();
	}

	m_canRenderAA = preparedAA;
}

void TemporalAAViewRenderSystem::PreRenderFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	Super::PreRenderFrame(graphBuilder, renderScene, viewSpec);

	if (m_canRenderAA)
	{
		SPT_CHECK(!!m_temporalAARenderer);
		SPT_CHECK(viewSpec.SupportsStage(ERenderStage::AntiAliasing))

		RenderStageEntries& stageEntries = viewSpec.GetRenderStageEntries(ERenderStage::AntiAliasing);
		stageEntries.GetOnRenderStage().AddRawMember(this, &TemporalAAViewRenderSystem::OnRenderAntiAliasingStage);
	}
}

void TemporalAAViewRenderSystem::OnInitialize(RenderView& inRenderView)
{
	Super::OnInitialize(inRenderView);

	inRenderView.AddRenderStages(ERenderStage::AntiAliasing);
}

void TemporalAAViewRenderSystem::OnRenderAntiAliasingStage(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_temporalAARenderer);
	SPT_CHECK(m_canRenderAA);

	const math::Vector2u outputRes = viewSpec.GetRenderView().GetRenderingRes();

	const RenderView& renderView = viewSpec.GetRenderView();
	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const rhi::EFragmentFormat texturesFormat = viewContext.luminance->GetFormat();

	const rg::RGTextureViewHandle outputColor = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Luminance Post AA"), rg::TextureDef(outputRes, texturesFormat));

	const rg::RGBufferViewHandle exposureBuffer = graphBuilder.AcquireExternalBufferView(renderView.GetExposureDataBuffer());

	const Uint32 exposureOffset	       = offsetof(ViewExposureData, exposure);
	const Uint32 historyExposureOffset = offsetof(ViewExposureData, exposureLastFrame);

	gfx::TemporalAARenderingParams renderingParams;
	renderingParams.depth                          = viewContext.depth;
	renderingParams.motion                         = viewContext.motion;
	renderingParams.inputColor                     = viewContext.luminance;
	renderingParams.outputColor                    = outputColor;
	renderingParams.jitter                         = renderView.GetJitter();
	renderingParams.sharpness                      = 0.0f;
	renderingParams.resetAccumulation              = false;
	renderingParams.exposure.exposureBuffer        = exposureBuffer;
	renderingParams.exposure.exposureOffset        = exposureOffset;
	renderingParams.exposure.historyExposureOffset = historyExposureOffset;

	m_temporalAARenderer->Render(graphBuilder, renderingParams);

	viewContext.luminance = outputColor;
}

} // spt::rsc
