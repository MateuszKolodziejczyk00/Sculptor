#if SPT_ENABLE_SCENE_RENDERER_STATS

#include "RTReflectionsStatsView.h"
#include "ImGui/SculptorImGui.h"


namespace spt::rsc
{

void RTReflectionsStatsView::DrawUI()
{
	SPT_PROFILER_FUNCTION();

	struct PlotData
	{
		lib::StaticArray<Real32, samplesNum>& data;
		SizeType                              startIdx;
	};

	const auto plotCallback = [](void* data, int idx) -> float
	{
		const PlotData& plotData = *static_cast<PlotData*>(data);
		return plotData.data[(plotData.startIdx + idx) % samplesNum];
	};

	ImGui::Text("First pass rays num:");

	const auto drawRaysNumber = [](const char* label, Uint32 raysNum, Real32 raysPercentage, const SamplesArray& samples)
	{
		ImGui::BeginColumns(label, 2);
		ImGui::Text("Num: %d", raysNum);
		ImGui::NextColumn();
		ImGui::Text("Percentage: %.2f%%", raysPercentage);
		ImGui::EndColumns();

		const Real32 min = *std::min_element(samples.begin(), samples.end());
		const Real32 max = *std::max_element(samples.begin(), samples.end());
		const Real32 avg = std::accumulate(samples.begin(), samples.end(), 0.f) / samples.size();

		ImGui::BeginColumns(label, 3);
		ImGui::Text("Min:");
		ImGui::Text("%.2f%%", min);
		ImGui::NextColumn();
		ImGui::Text("Max:");
		ImGui::Text("%.2f%%", max);
		ImGui::NextColumn();
		ImGui::Text("Average:");
		ImGui::Text("%.2f%%", avg);
		ImGui::EndColumns();
	};

	{
		const lib::LockGuard lock(m_samplesLock);

		PlotData firstPassRaysNumPlotData{ m_firstPassRaysNum, m_currentSampleIndex };
		ImGui::PlotLines("##FirstPassRaysNum", plotCallback, &firstPassRaysNumPlotData, samplesNum, 0, nullptr, 0.f, 100.f, math::Vector2f(0, 80));

		drawRaysNumber("First Pass Rays Info", m_lastFirstPassRaysNum, m_lastFirstPassRaysPercentage, m_firstPassRaysNum);

		ImGui::Separator();

		ImGui::Text("Second pass rays num:");

		PlotData secondPassRaysNumPlotData{ m_secondPassRaysNum, m_currentSampleIndex };
		ImGui::PlotLines("##SecondPassRaysNum", plotCallback, &secondPassRaysNumPlotData, samplesNum, 0, nullptr, 0.f, 100.f, math::Vector2f(0, 80));

		drawRaysNumber("Second Pass Rays Info", m_lastSecondPassRaysNum, m_lastSecondPassRaysPercentage, m_secondPassRaysNum);
	}
}

void RTReflectionsStatsView::RecordFrameSample(const FrameSample& frameSample)
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard lock(m_samplesLock);

	const Real32 pixelsNum = static_cast<Real32>(frameSample.resolution.x() * frameSample.resolution.y());

	m_firstPassRaysNum[m_currentSampleIndex]  = static_cast<Real32>(frameSample.numRaysFirstPass) / pixelsNum * 100.f;
	m_secondPassRaysNum[m_currentSampleIndex] = static_cast<Real32>(frameSample.numRaysSecondPass) / pixelsNum * 100.f;

	m_lastFirstPassRaysNum        = frameSample.numRaysFirstPass;
	m_lastFirstPassRaysPercentage = m_firstPassRaysNum[m_currentSampleIndex];

	m_lastSecondPassRaysNum        = frameSample.numRaysSecondPass;
	m_lastSecondPassRaysPercentage = m_secondPassRaysNum[m_currentSampleIndex];

	m_currentSampleIndex = (m_currentSampleIndex + 1) % samplesNum;
}

} // spt::rsc

#endif // SPT_ENABLE_SCENE_RENDERER_STATS
