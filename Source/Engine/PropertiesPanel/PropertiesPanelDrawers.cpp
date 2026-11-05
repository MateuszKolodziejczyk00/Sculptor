#include "PropertiesPanelUtils.h"
#include "ImGui/SculptorImGui.h"


namespace spt::ui
{

template<typename TType>
struct ScalarDrawInfo
{
	TType& value;

	const TType* minValue;
	const TType* maxValue;
};


template<typename TType>
ScalarDrawInfo<TType> MakeScalarDrawInfo(const rtti::RTTIMember& member, lib::Span<Byte> data)
{
	SPT_CHECK(data.size() >= sizeof(TType));

	return ScalarDrawInfo<TType>
	{
		.value    = *reinterpret_cast<TType*>(data.data()),
		.minValue = static_cast<const TType*>(member.minValue),
		.maxValue = static_cast<const TType*>(member.maxValue),
	};
}


template<typename TType>
static Bool DrawScalar(const ScalarDrawInfo<TType>& scalarInfo, Real32 dragSpeed)
{
	ImGui::SetNextItemWidth(-FLT_MIN);

	ImGuiDataType dataType;
	if constexpr (std::is_same_v<TType, Real32>)
	{
		dataType = ImGuiDataType_Float;
	}
	else if constexpr (std::is_same_v<TType, Int32>)
	{
		dataType = ImGuiDataType_S32;
	}
	else if constexpr (std::is_same_v<TType, Uint32>)
	{
		dataType = ImGuiDataType_U32;
	}
	else
	{
		SPT_CHECK_NO_ENTRY();
	}

	if (!scalarInfo.minValue || !scalarInfo.maxValue)
	{
		return ImGui::DragScalar("##Value", dataType, &scalarInfo.value, dragSpeed, scalarInfo.minValue, scalarInfo.maxValue);
	}

	const TType min = *scalarInfo.minValue;
	const TType max = *scalarInfo.maxValue;
	SPT_CHECK(max > min);

	const Bool changed = ImGui::SliderScalar("##Value", dataType, &scalarInfo.value, &min, &max);

	const TType value     = scalarInfo.value;
	const Real64 fraction = std::clamp((Real64(value) - Real64(min)) / (Real64(max) - Real64(min)), 0.0, 1.0);

	const ImVec2 frameMin = ImGui::GetItemRectMin();
	const ImVec2 frameMax = ImGui::GetItemRectMax();
	ImGui::GetWindowDrawList()->AddRectFilled(frameMin,
											  ImVec2(ImLerp(frameMin.x, frameMax.x, Real32(fraction)), frameMax.y),
											  ImGui::GetColorU32(ImGuiCol_SliderGrab, 0.15f),
											  ImGui::GetStyle().FrameRounding);

	return changed;
}


template<typename TType>
static Bool ScalarPropertyDrawer(const PropertyDrawContext& context, const rtti::RTTIMember& member, lib::StringView displayName, lib::Span<Byte> data)
{
	DrawPropertyName(context, member, displayName, data);
	return DrawScalar(MakeScalarDrawInfo<TType>(member, data), std::is_floating_point_v<TType> ? 0.01f : 1.f);
}

SPT_REGISTER_PROPERTY_DRAWER(Uint32, ScalarPropertyDrawer<Uint32>);
SPT_REGISTER_PROPERTY_DRAWER(Int32,  ScalarPropertyDrawer<Int32>);
SPT_REGISTER_PROPERTY_DRAWER(Real32, ScalarPropertyDrawer<Real32>);


static Bool BoolPropertyDrawer(const PropertyDrawContext& context, const rtti::RTTIMember& member, lib::StringView displayName, lib::Span<Byte> data)
{
	DrawPropertyName(context, member, displayName, data);
	return ImGui::Checkbox("##Value", reinterpret_cast<Bool*>(data.data()));
}


SPT_REGISTER_PROPERTY_DRAWER(Bool, BoolPropertyDrawer);


static Bool Vector3PropertyDrawer(const PropertyDrawContext& context, const rtti::RTTIMember& member, lib::StringView displayName, lib::Span<Byte> data)
{
	DrawPropertyName(context, member, displayName, data);
	if (member.HasMetaData("color"))
	{
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::ColorEdit3("##Value", reinterpret_cast<math::Vector3f*>(data.data())->data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
	}
	else
	{
		ImGui::SetNextItemWidth(-FLT_MIN);
		return ImGui::DragFloat3("##Value", reinterpret_cast<math::Vector3f*>(data.data())->data(), 0.01f);
	}
}


SPT_REGISTER_PROPERTY_DRAWER(math::Vector3f, Vector3PropertyDrawer);

} // spt::ui
