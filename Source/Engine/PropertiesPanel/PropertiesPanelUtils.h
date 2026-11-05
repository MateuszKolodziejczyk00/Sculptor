#pragma once

#include "PropertiesPanelMacros.h"
#include "SculptorCoreTypes.h"
#include "RTTI.h"
#include "Utility/Templates/Callable.h"


namespace spt::ui
{

struct PropertyDrawContext
{
	lib::StringView filter;
};


using PropertyDrawer = lib::RawCallable<Bool(const PropertyDrawContext& context, const rtti::RTTIMember& member, lib::StringView displayName, lib::Span<Byte> data)>;

PROPERTIES_PANEL_API void RegisterPropertyDrawer(const lib::RuntimeTypeInfo& type, PropertyDrawer drawer);
PROPERTIES_PANEL_API PropertyDrawer GetPropertyDrawer(const lib::RuntimeTypeInfo& type);

PROPERTIES_PANEL_API void DrawTypeTooltip(const lib::RuntimeTypeInfo& type);
PROPERTIES_PANEL_API void DrawPropertyName(const PropertyDrawContext& context, const rtti::RTTIMember& member, lib::StringView displayName, lib::Span<Byte> data);

template<typename TType>
struct PropertyDrawerRegisrar
{
	explicit PropertyDrawerRegisrar(PropertyDrawer drawer)
	{
		RegisterPropertyDrawer(lib::TypeInfo<TType>{}, std::move(drawer));
	}
};

} // spt::ui

#define SPT_REGISTER_PROPERTY_DRAWER(type, drawer) \
	inline const spt::ui::PropertyDrawerRegisrar<type> SPT_IDENTIFIER_NAME(PropDrawerRegistration)(drawer);
