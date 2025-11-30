#pragma once

#include "SculptorAliases.h"
#include "Utility/TypeID.h"
#include "Containers/HashMap.h"
#include "Assertions/Assertions.h"
#include "Utility/Memory.h"
#include "Serialization.h"


namespace spt::lib
{

class BlackboardElementBase
{
public:

	BlackboardElementBase() = default;

	virtual ~BlackboardElementBase()
	{
	}
};


template<typename TDataType>
class BlackboardElement : public BlackboardElementBase
{
public:

	using DataType = TDataType;
	BlackboardElement() = default;

	template<typename... TArgs>
	BlackboardElement(TArgs&&... args)
		: m_data(std::forward<TArgs>(args)...)
	{
	}

	template<typename TType>
	BlackboardElement& operator=(TType&& rhs)
	{
		m_data = std::forward<TType>(rhs);
		return *this;
	}

	TDataType& Get()
	{
		return m_data;
	}

	const TDataType& Get() const
	{
		return m_data;
	}

private:

	TDataType m_data;
};


class Blackboard
{
public:

	Blackboard() = default;

	template<typename TDataType, typename... TArgs>
	TDataType& Create(TArgs&&... args)
	{
		const auto [it, wasEmplaced] = m_data.emplace(TypeInfo<TDataType>(), nullptr);
		SPT_CHECK(wasEmplaced);

		lib::UniquePtr<BlackboardElement<TDataType>> elem = std::make_unique<BlackboardElement<TDataType>>(std::forward<TArgs>(args)...);
		BlackboardElement<TDataType>& elemRef = *elem;

		it->second = std::move(elem);

		return elemRef.Get();
	}

	template<typename TDataType, typename TValueType>
	void Set(TValueType&& newValue)
	{
		GetElementOrCreate<TDataType>() = std::forward<TValueType>(newValue);
	}

	template<typename TDataType>
	TDataType& GetOrCreate()
	{
		return GetElementOrCreate<TDataType>().Get();
	}
	
	template<typename TDataType>
	TDataType& Get()
	{
		return GetElement<TDataType>().Get();
	}

	template<typename TDataType>
	const TDataType& Get() const
	{
		return GetElement<TDataType>().Get();
	}
	
	template<typename TDataType>
	Bool Contains() const
	{
		return m_data.contains(TypeInfo<TDataType>());
	}
	
	template<typename TDataType>
	TDataType* Find()
	{
		BlackboardElement<TDataType>* foundData = TryGetElement<TDataType>();
		return foundData ? &foundData->Get() : nullptr;
	}
	
	template<typename TDataType>
	const TDataType* Find() const
	{
		const BlackboardElement<TDataType>* foundData = TryGetElement<TDataType>();
		return foundData ? &foundData->Get() : nullptr;
	}

	void Remove(lib::RuntimeTypeInfo type)
	{
		m_data.erase(type);
	}

	Bool MoveType(Blackboard& rhs, lib::RuntimeTypeInfo type)
	{
		auto foundRhsElem = rhs.m_data.find(type);
		if (foundRhsElem != rhs.m_data.end())
		{
			m_data[type] = std::move(foundRhsElem->second);
			rhs.m_data.erase(foundRhsElem);

			return true;
		}

		return false;
	}

	template<typename TCallable>
	void ForEachType(TCallable&& callable) const
	{
		for (const auto& [type, data] : m_data)
		{
			callable(type);
		}
	}

	void Serialize(srl::Serializer& serializer);

private:

	template<typename TType>
	BlackboardElement<TType>& GetElementOrCreate()
	{
		lib::UniquePtr<BlackboardElementBase>& elem = m_data[TypeInfo<TType>()];
		if (!elem)
		{
			elem = std::make_unique<BlackboardElement<TType>>();
		}

		return reinterpret_cast<BlackboardElement<TType>&>(*elem);
	}

	template<typename TType>
	BlackboardElement<TType>& GetElement()
	{
		return reinterpret_cast<BlackboardElement<TType>&>(*m_data.at(TypeInfo<TType>()));
	}

	template<typename TType>
	const BlackboardElement<TType>& GetElement() const
	{
		return reinterpret_cast<const BlackboardElement<TType>&>(*m_data.at(TypeInfo<TType>()));
	}

	template<typename TType>
	const BlackboardElement<TType>* TryGetElement() const
	{
		const auto foundData = m_data.find(TypeInfo<TType>());
		return foundData != m_data.cend() ? &reinterpret_cast<const BlackboardElement<TType>&>(*foundData->second) : nullptr;
	}

	template<typename TType>
	BlackboardElement<TType>* TryGetElement()
	{
		const auto foundData = m_data.find(TypeInfo<TType>());
		return foundData != m_data.cend() ? &reinterpret_cast<BlackboardElement<TType>&>(*foundData->second) : nullptr;
	}

	lib::HashMap<RuntimeTypeInfo, lib::UniquePtr<BlackboardElementBase>> m_data;
};

} // spt::lib
