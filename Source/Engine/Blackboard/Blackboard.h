#pragma once

#include "SculptorAliases.h"
#include "Utility/TypeID.h"
#include "Containers/HashMap.h"
#include "Assertions/Assertions.h"


namespace spt::lib
{

class BlackboardElement
{
public:

	BlackboardElement()
		: m_dataPtr(nullptr)
		, m_deleter(nullptr)
		, m_moveOperator(nullptr)
	{ }

	~BlackboardElement()
	{
		Delete();
	}

	BlackboardElement(BlackboardElement&& other)
	{
		Move(std::move(other));
	}

	BlackboardElement& operator=(BlackboardElement&& other)
	{
		Delete();
		Move(std::move(other));
	}

	Bool IsValid() const
	{
		return !!m_dataPtr;
	}

	template<typename TDataType, typename... TArgs>
	TDataType& Create(TArgs&&... args)
	{
		SPT_CHECK(!IsValid());
		CreateImpl<TDataType>(std::forward<TArgs>(args)...);
		return Get<TDataType>();
	}

	template<typename TDataType, typename TValueType>
	void Set(TValueType&& value)
	{
		if (m_dataPtr)
		{
			TDataType* dataPtr = reinterpret_cast<TDataType*>(m_dataPtr);
			*dataPtr = std::forward<TValueType>(value);
		}
		else
		{
			CreateImpl<TDataType>(std::forward<TValueType>(value));
		}
	}
	
	template<typename TDataType>
	TDataType& GetOrCreate()
	{
		if (!IsValid())
		{
			CreateImpl<TDataType>();
		}
		return Get<TDataType>();
	}

	template<typename TDataType>
	TDataType& Get()
	{
		SPT_CHECK(!!m_dataPtr);
		return *reinterpret_cast<TDataType*>(m_dataPtr);
	}

	template<typename TDataType>
	const TDataType& Get() const
	{
		SPT_CHECK(!!m_dataPtr);
		return *reinterpret_cast<const TDataType*>(m_dataPtr);
	}

private:

	Bool IsAllocatedInline() const
	{
		return m_dataPtr == m_inlineStorage;
	}

	template<typename TDataType, typename... TArgs>
	void CreateImpl(TArgs&&... args)
	{
		SPT_STATIC_CHECK_MSG(alignof(TDataType) <= 16, "Currently alignments higher than 16 are not handled");
		
		if constexpr (sizeof(TDataType) <= inlineSize)
		{
			m_dataPtr = new (m_inlineStorage) TDataType(std::forward<TArgs>(args)...);
		}
		else
		{
			m_dataPtr = new TDataType(std::forward<TArgs>(args)...);
		}

		if constexpr (!std::is_trivial_v<TDataType>)
		{
			m_deleter = [](void* data) { reinterpret_cast<TDataType*>(data)->~TDataType(); };
		}

		m_moveOperator = [](void* dst, void* src) { *reinterpret_cast<TDataType*>(dst) = std::move(*reinterpret_cast<TDataType*>(src)); };
	}

	void Delete()
	{
		if (m_dataPtr)
		{
			if (m_deleter)
			{
				m_deleter(m_dataPtr);
				m_deleter = nullptr;
			}

			if (!IsAllocatedInline())
			{
				delete m_dataPtr;
			}

			m_dataPtr = nullptr;
			m_moveOperator = nullptr;
		}
	}

	void Move(BlackboardElement&& other)
	{
		SPT_CHECK(IsValid() && other.IsValid());

		if (!other.IsAllocatedInline())
		{
			m_dataPtr = other.m_dataPtr;
			m_deleter = other.m_deleter;
			m_moveOperator = other.m_moveOperator;
			other.m_dataPtr = nullptr;
			other.m_deleter = nullptr;
			other.m_moveOperator = nullptr;
		}
		else
		{
			m_moveOperator(m_dataPtr, other.m_dataPtr);
		}
	}

	static constexpr SizeType inlineSize = 64;
	alignas(16) Byte m_inlineStorage[inlineSize];

	void* m_dataPtr;
	void(*m_deleter)(void* /*data*/);
	void(*m_moveOperator)(void* /*dst*/, void* /*src*/);
};


class Blackboard
{
public:

	Blackboard() = default;

	template<typename TDataType, typename... TArgs>
	TDataType& Create(TArgs&&... args)
	{
		return m_data[TypeInfo<TDataType>()].Create<TDataType>(std::forward<TArgs>(args)...);
	}

	template<typename TDataType, typename TValueType>
	void Set(TValueType&& newValue)
	{
		m_data[TypeInfo<TDataType>()].Set<TDataType>(std::forward<TValueType>(newValue));
	}

	template<typename TDataType>
	TDataType& GetOrCreate()
	{
		return m_data[TypeInfo<TDataType>()].GetOrCreate<TDataType>();
	}
	
	template<typename TDataType>
	TDataType& Get()
	{
		return m_data.at(TypeInfo<TDataType>()).Get<TDataType>();
	}

	template<typename TDataType>
	const TDataType& Get() const
	{
		return m_data.at(TypeInfo<TDataType>()).Get<TDataType>();
	}
	
	template<typename TDataType>
	Bool Contains() const
	{
		return m_data.contains(TypeInfo<TDataType>());
	}
	
	template<typename TDataType>
	TDataType* Find()
	{
		const auto foundData = m_data.find(TypeInfo<TDataType>());
		return foundData != std::end(m_data) ? &foundData->second.Get<TDataType>() : nullptr;
	}
	
	template<typename TDataType>
	const TDataType* Find() const
	{
		const auto foundData = m_data.find(TypeInfo<TDataType>());
		return foundData != std::cend(m_data) ? &foundData->second.Get<TDataType>() : nullptr;
	}

	template<typename TCallable>
	void ForEachType(TCallable&& callable) const
	{
		for (const auto& [type, data] : m_data)
		{
			callable(type);
		}
	}

private:

	lib::HashMap<RuntimeTypeInfo, BlackboardElement> m_data;
};

} // spt::lib
