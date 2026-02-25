#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rsc
{

class RENDER_SCENE_API RendererParamsRegistry
{
public:

	static void RegisterParameter(const lib::DynamicArray<lib::String>& category, class RendererParamBase& param);

	static void DrawParametersUI();
};


class RendererParamBase
{
public:

	virtual ~RendererParamBase() = default;

	virtual void DrawUI() = 0;
};


template<typename TParam, typename TType>
class RendererParameter : public RendererParamBase
{
public:

	void SetValue(TType value)
	{
		paramValue = value;
	}

	const TType& GetValue() const
	{
		return paramValue;
	}

	operator TType() const
	{
		return paramValue;
	}

	// Begin RendererParamBase overrides
	virtual void DrawUI() final
	{
		TParam* selfAsParam = static_cast<TParam*>(this);
		selfAsParam->DrawUI(paramName, paramValue);
	}
	// End RendererParamBase overrides

protected:

	RendererParameter(const lib::String& inName, const lib::DynamicArray<lib::String>& inCategory, const TType& defaultValue)
		: paramName(inName)
		, paramValue(defaultValue)
	{
		RendererParamsRegistry::RegisterParameter(inCategory, *this);
	}

private:

	lib::String paramName;
	TType paramValue;
};


class RendererFloatParameter : public RendererParameter<RendererFloatParameter, Real32>
{
protected:
	
	using Super = RendererParameter<RendererFloatParameter, Real32>;

public:

	RendererFloatParameter(const lib::String& name, const lib::DynamicArray<lib::String>& category, Real32 defaultValue, Real32 inMin, Real32 inMax);

	void DrawUI(const lib::String& name, Real32& value);

private:

	Real32 min;
	Real32 max;
};


class RendererFloat3Parameter : public RendererParameter<RendererFloat3Parameter, math::Vector3f>
{
protected:

	using Super = RendererParameter<RendererFloat3Parameter, math::Vector3f>;

public:

	RendererFloat3Parameter(const lib::String& name, const lib::DynamicArray<lib::String>& category, math::Vector3f defaultValue, Real32 inMin, Real32 inMax);

	void DrawUI(const lib::String& name, math::Vector3f& value);

private:

	Real32 min;
	Real32 max;
};


class RendererBoolParameter : public RendererParameter<RendererBoolParameter, Bool>
{
protected:

	using Super = RendererParameter<RendererBoolParameter, Bool>;

public:

	RendererBoolParameter(const lib::String& name, const lib::DynamicArray<lib::String>& category, Bool defaultValue);

	void DrawUI(const lib::String& name, Bool& value);
};


class RendererIntParameter : public RendererParameter<RendererIntParameter, Int32>
{
protected:

	using Super = RendererParameter<RendererIntParameter, Int32>;

public:

	RendererIntParameter(const lib::String& name, const lib::DynamicArray<lib::String>& category, Int32 defaultValue, Int32 inMin, Int32 inMax);

	void DrawUI(const lib::String& name, Int32& value);

private:

	Int32 min;
	Int32 max;
};

} // spt::rsc
