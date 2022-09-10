#pragma once

#include "Types/DescriptorSetState.h"


namespace spt::rdr
{

template<const char* propertyName, typename TPropertyType>
class DescriptorSetPropertyState : public DescriptorSetState
{
public:

	using PropertyType = TPropertyType;

	template<typename... TArgs>
	DescriptorSetPropertyState(TArgs&&... args)
		: m_property(std::forward<TArgs>(args)...)
		, m_name(propertyName)
	{
		MarkAsDirty();
	}

	template<typename TSetter> requires std::is_assignable_v<PropertyType&, TSetter>
	void Set(TSetter&& setter)
	{
		m_property = std::forward<TSetter>(setter);
		MarkAsDirty();
	}

	template<typename TSetter> requires std::is_assignable_v<PropertyType&, std::invoke_result_t<const TSetter&>>
	void Set(const TSetter& setter)
	{
		m_property = std::invoke(setter);
		MarkAsDirty();
	}

	template<typename TSetter> requires std::is_invocable_v<const TSetter&, PropertyType&>
	void Set(const TSetter& setter)
	{
		std::invoke(setter, m_property);
		MarkAsDirty();
	}

	const lib::HashedString& GetName() const
	{
		return m_name;
	}

	const PropertyType& GetValue() const
	{
		return m_property;
	}

private:

	PropertyType		m_property;
	lib::HashedString	m_name;
};

} // spt::rdr