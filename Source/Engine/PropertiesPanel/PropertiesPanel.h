#pragma once

#include "PropertiesPanelMacros.h"
#include "SculptorCoreTypes.h"
#include "RTTI.h"
#include "Utility/Templates/Callable.h"


namespace spt::ui
{

struct Property
{
	template<typename TType>
	Property(lib::StringView inName, TType& inData)
		: name(inName)
		, type(lib::TypeInfo<TType>{})
		, data(reinterpret_cast<Byte*>(&inData), sizeof(TType))
	{
	}

	lib::StringView      name;
	lib::RuntimeTypeInfo type;
	lib::Span<Byte>      data;
};


PROPERTIES_PANEL_API Bool DrawPropertiesPanel(const lib::RuntimeTypeInfo& type, lib::Span<Byte> data);

PROPERTIES_PANEL_API Bool DrawPropertiesPanel(lib::StringView name, const lib::RuntimeTypeInfo& type, lib::Span<Byte> data);

PROPERTIES_PANEL_API Bool DrawPropertiesPanel(lib::StringView name, lib::Span<const Property> properties);

template<rtti::CRTTIStruct TType>
Bool DrawPropertiesPanel(TType& data)
{
	return DrawPropertiesPanel(lib::TypeInfo<TType>{}, lib::Span<Byte>(reinterpret_cast<Byte*>(&data), sizeof(TType)));
}

template<rtti::CRTTIStruct TType>
Bool DrawPropertiesPanel(lib::StringView name, TType& data)
{
	return DrawPropertiesPanel(name, lib::TypeInfo<TType>{}, lib::Span<Byte>(reinterpret_cast<Byte*>(&data), sizeof(TType)));
}

template<typename... TProperties>
	requires (std::same_as<TProperties, Property> && ...)
Bool DrawPropertiesPanel(lib::StringView name, const TProperties&... properties)
{
	const Property propertiesArray[] = { properties... };
	return DrawPropertiesPanel(name, lib::Span<const Property>(propertiesArray));
}

} // namespace spt::ui
