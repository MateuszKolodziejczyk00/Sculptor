#include "SceneRendererParams.h"
#include "ImGui/SculptorImGui.h"

namespace spt::rsc
{

namespace constants
{
static constexpr Real32 indentationValue = 24.f;
} // constants

//////////////////////////////////////////////////////////////////////////////////////////////////
// ParametersContainer ===========================================================================

namespace priv
{

struct ParametersContainer
{
	struct Node
	{
		Node() = default;

		explicit Node(const lib::String& name)
			: categoryName(name)
		{ }

		lib::String categoryName;
		lib::DynamicArray<Node> children;
		lib::DynamicArray<RendererParamBase*> params;
	};

	ParametersContainer() = default;

	void DrawUI() const
	{
		if (ImGui::CollapsingHeader("Scene Renderer Parameters"))
		{
			for (const Node& node : m_nodes)
			{
				DrawNodeUI(node);
			}
		}
	}

	void AddParam(const lib::DynamicArray<lib::String>& category, RendererParamBase& param)
	{
		Node& node = GetNode(category);

		node.params.emplace_back(&param);
	}

private:

	Node& GetNode(const lib::DynamicArray<lib::String>& category)
	{
		SPT_CHECK(!category.empty());
		
		Node* currentNode = nullptr;

		for (SizeType subcategoryIdx = 0; subcategoryIdx < category.size(); ++subcategoryIdx)
		{
			lib::DynamicArray<Node>& childrenRef = currentNode != nullptr ? currentNode->children : m_nodes;

			const auto foundNode = std::find_if(std::begin(childrenRef), std::end(childrenRef), [categoryName = category[subcategoryIdx]](const Node& node) { return node.categoryName == categoryName; });

			if (foundNode != std::end(childrenRef))
			{
				currentNode = &*foundNode;
			}
			else
			{
				currentNode = &childrenRef.emplace_back(Node(category[subcategoryIdx]));
			}
		}

		SPT_CHECK(!!currentNode);
		return *currentNode;
	}
	
	void DrawNodeUI(const Node& node) const
	{
		ImGui::Indent(constants::indentationValue);
		if (ImGui::CollapsingHeader(node.categoryName.data()))
		{

			for (const Node& child : node.children)
			{
				DrawNodeUI(child);
			}

			for (RendererParamBase* param : node.params)
			{
				param->DrawUI();
			}
		}
		ImGui::Unindent(constants::indentationValue);
	}

	lib::DynamicArray<Node> m_nodes;
};

static ParametersContainer& GetContainer()
{
	static ParametersContainer container;
	return container;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// RendererParamsRegistry ========================================================================

void RendererParamsRegistry::RegisterParameter(const lib::DynamicArray<lib::String>& category, class RendererParamBase& param)
{
	priv::ParametersContainer& container = priv::GetContainer();
	container.AddParam(category , param);
}

void RendererParamsRegistry::DrawParametersUI()
{
	SPT_PROFILER_FUNCTION();

	const priv::ParametersContainer& container = priv::GetContainer();
	container.DrawUI();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RendererFloatParameter ========================================================================

RendererFloatParameter::RendererFloatParameter(const lib::String& name, const lib::DynamicArray<lib::String>& category, Real32 defaultValue, Real32 inMin, Real32 inMax)
	: Super(name, category, defaultValue)
	, min(inMin)
	, max(inMax)
{ }

void RendererFloatParameter::DrawUI(const lib::String& name, Real32& value)
{
	ImGui::SliderFloat(name.data(), &value, min, max, "%.5f");
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RendererFloat3Parameter =========================================================================

RendererFloat3Parameter::RendererFloat3Parameter(const lib::String& name, const lib::DynamicArray<lib::String>& category, math::Vector3f defaultValue, Real32 inMin, Real32 inMax)
	: Super(name, category, defaultValue)
	, min(inMin)
	, max(inMax)
{ }

void RendererFloat3Parameter::DrawUI(const lib::String& name, math::Vector3f& value)
{
	ImGui::SliderFloat3(name.data(), value.data(), min, max);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RendererBoolParameter =========================================================================

RendererBoolParameter::RendererBoolParameter(const lib::String& name, const lib::DynamicArray<lib::String>& category, Bool defaultValue)
	: Super(name, category, defaultValue)
{ }

void RendererBoolParameter::DrawUI(const lib::String& name, Bool& value)
{
	ImGui::Checkbox(name.data(), &value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RendererIntParameter ==========================================================================

RendererIntParameter::RendererIntParameter(const lib::String& name, const lib::DynamicArray<lib::String>& category, Int32 defaultValue, Int32 inMin, Int32 inMax)
	: Super(name, category, defaultValue)
	, min(inMin)
	, max(inMax)
{ }

void RendererIntParameter::DrawUI(const lib::String& name, Int32& value)
{
	ImGui::SliderInt(name.data(), &value, min, max);
}

} // spt::rsc
