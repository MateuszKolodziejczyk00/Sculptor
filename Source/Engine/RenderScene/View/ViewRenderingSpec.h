#pragma once

#include "Delegates/MulticastDelegate.h"
#include "SceneRenderingTypes.h"
#include "SculptorECS.h"
#include "RGResources/RGTrackedObject.h"

namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class RenderView;
class ViewRenderingSpec;


class ViewRenderingDataElement
{
public:

	ViewRenderingDataElement()
		: m_dataPtr(nullptr)
		, m_deleter(nullptr)
		, m_moveOperator(nullptr)
	{ }

	~ViewRenderingDataElement()
	{
		Delete();
	}

	ViewRenderingDataElement(ViewRenderingDataElement&& other)
	{
		Move(std::move(other));
	}

	ViewRenderingDataElement& operator=(ViewRenderingDataElement&& other)
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
			CreateImpl<TDataType>(std::forward_as_tuple<TValueType>(value));
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
		SPT_STATIC_CHECK_MSG(alignof(TDataType) <= 16, "Currently alignments higher thatn 16 are not handled");
		
		if constexpr (sizeof(TDataType) <= inlineSize)
		{
			m_dataPtr = new (m_inlineStorage) TDataType(std::forward<TArgs>(args)...);
		}
		else
		{
			m_dataPtr = new TDataType(std::forward<TArgs>(args)...);
		}

		if constexpr (!std::is_pod_v<TDataType>)
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

	void Move(ViewRenderingDataElement&& other)
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


class ViewRenderingDataContainer
{
public:

	ViewRenderingDataContainer() = default;

	template<typename TDataType, typename... TArgs>
	TDataType& Create(TArgs&&... args)
	{
		return m_data[ecs::type_id<TDataType>()].Create<TDataType>(std::forward<TArgs>(args)...);
	}

	template<typename TDataType, typename TValueType>
	void Set(TValueType&& newValue)
	{
		m_data[ecs::type_id<TDataType>()].Set<TDataType>(std::forward<TValueType>(newValue));
	}

	template<typename TDataType>
	TDataType& GetOrCreate()
	{
		return m_data[ecs::type_id<TDataType>()].GetOrCreate<TDataType>();
	}
	
	template<typename TDataType>
	TDataType& Get()
	{
		return m_data.at(ecs::type_id<TDataType>()).Get<TDataType>();
	}

	template<typename TDataType>
	const TDataType& Get() const
	{
		return m_data.at(ecs::type_id<TDataType>()).Get<TDataType>();
	}
	
	template<typename TDataType>
	Bool Contains() const
	{
		return m_data.contains(ecs::type_id<TDataType>());
	}
	
	template<typename TDataType>
	TDataType* Find()
	{
		const auto foundData = m_data.find(ecs::type_id<TDataType>());
		return foundData != std::end(m_data) ? &foundData->second.Get<TDataType>() : nullptr;
	}
	
	template<typename TDataType>
	const TDataType* Find() const
	{
		const auto foundData = m_data.find(ecs::type_id<TDataType>());
		return foundData != std::cend(m_data) ? &foundData->second.Get<TDataType>() : nullptr;
	}

private:

	using TypeID = ecs::type_info;

	lib::HashMap<TypeID, ViewRenderingDataElement> m_data;
};


struct RenderStageExecutionContext
{
	explicit RenderStageExecutionContext(ERenderStage inStage)
		: stage(inStage)
	{ }

	ERenderStage stage;
};


class RenderStageEntries
{
public:

	using PreRenderStageDelegate	= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, ViewRenderingSpec& /*viewSpec*/, const RenderStageExecutionContext& /*context*/)>;
	using OnRenderStageDelegate		= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, ViewRenderingSpec& /*viewSpec*/, const RenderStageExecutionContext& /*context*/)>;
	using PostRenderStageDelegate	= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, ViewRenderingSpec& /*viewSpec*/, const RenderStageExecutionContext& /*context*/)>;

	RenderStageEntries();

	PreRenderStageDelegate&		GetPreRenderStage();
	OnRenderStageDelegate&		GetOnRenderStage();
	PostRenderStageDelegate&	GetPostRenderStage();

private:

	PreRenderStageDelegate  m_preRenderStage;
	OnRenderStageDelegate	m_onRenderStage;
	PostRenderStageDelegate	m_postRenderStage;
};


class ViewRenderingSpec : public rg::RGTrackedObject
{
public:

	ViewRenderingSpec();

	void Initialize(RenderView& renderView);
	
	RenderView& GetRenderView() const;
	ERenderStage GetSupportedStages() const;
	Bool SupportsStage(ERenderStage stage) const;

	const ViewRenderingDataContainer&	GetData() const;
	ViewRenderingDataContainer&			GetData();

	const RenderStageEntries&	GetRenderStageEntries(ERenderStage stage) const;
	RenderStageEntries&			GetRenderStageEntries(ERenderStage stage);

private:

	lib::HashMap<SizeType, RenderStageEntries> m_stagesEntries;

	ViewRenderingDataContainer m_viewRenderingData;

	RenderView* m_renderView;
};

} // spt::rsc