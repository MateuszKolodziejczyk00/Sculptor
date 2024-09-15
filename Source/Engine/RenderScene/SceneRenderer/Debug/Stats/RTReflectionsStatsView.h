#pragma once

#if SPT_ENABLE_SCENE_RENDERER_STATS

#include "SceneRendererStatsView.h"


namespace spt::rsc
{

class RTReflectionsStatsView : public SceneRendererStatsView
{
public:

	struct FrameSample
	{
		math::Vector2u resolution        = {};
		Uint32         numRaysFirstPass  = 0u;
		Uint32         numRaysSecondPass = 0u;
	};

	RTReflectionsStatsView() = default;
	virtual ~RTReflectionsStatsView() = default;

	// Begin SceneRendererStatsView overrides
	virtual void DrawUI() override;
	// End SceneRendererStatsView overrides

	void RecordFrameSample(const FrameSample& frameSample);

private:

	constexpr static SizeType samplesNum = 100u;

	lib::StaticArray<Real32, samplesNum> m_firstPassRaysNum;
	lib::StaticArray<Real32, samplesNum> m_secondPassRaysNum;

	Uint32 m_lastFirstPassRaysNum        = 0u;
	Real32 m_lastFirstPassRaysPercentage = 0.f;

	Uint32 m_lastSecondPassRaysNum        = 0u;
	Real32 m_lastSecondPassRaysPercentage = 0.f;

	SizeType m_currentSampleIndex = 0u;

	lib::Lock m_samplesLock;
};

} // spt::rsc

#endif // SPT_ENABLE_SCENE_RENDERER_STATS
