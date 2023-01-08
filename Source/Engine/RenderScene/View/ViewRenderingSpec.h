#pragma once

#include "Delegates/MulticastDelegate.h"
#include "SceneRenderingTypes.h"
#include "SculptorECS.h"

namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class RenderView;


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
			SPT_STATIC_CHECK_MSG(alignof(TDataType) <= 16, "Currently alignments higher thatn 16 are not handled");
			if (sizeof(TDataType) <= inlineSize)
			{
				new (m_inlineStorage) TDataType(std::forward<TValueType>(value));
			}
			else
			{
				m_dataPtr = new TDataType(std::forward<TValueType>(value));
			}

			if constexpr (!std::is_pod_v<TDataType>)
			{
				m_deleter = [](void* data) { ~reinterpret_cast<TDataType*>(data); };
			}

			m_moveOperator = [](void* dst, void* src) { reinterpret_cast<TDataType*>(dst) = std::move(*reinterpret_cast<TDataType*>(src)); };
		}
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

	template<typename TDataType, typename TValueType>
	void Set(TValueType&& newValue)
	{
		m_data[ecs::type_id<TDataType>()].Set<TDataType>(std::forward<TValueType>(newValue));
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


class RenderStageEntries
{
public:

	using PreRenderStageDelegate	= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, const RenderView& /*view*/)>;
	using OnRenderStageDelegate		= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, const RenderView& /*view*/)>;
	using PostRenderStageDelegate	= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, const RenderView& /*view*/)>;

	RenderStageEntries();

	PreRenderStageDelegate&		GetPreRenderStageDelegate();
	OnRenderStageDelegate&		GetOnRenderStage();
	PostRenderStageDelegate&	GetPostRenderStage();

private:

	PreRenderStageDelegate  m_preRenderStage;
	OnRenderStageDelegate	m_onRenderStage;
	PostRenderStageDelegate	m_postRenderStage;
};


class ViewRenderingSpec
{
public:

	ViewRenderingSpec();

	void Initialize(const RenderView& renderView);
	
	const RenderView& GetRenderView() const;
	ERenderStage GetSupportedStages() const;
	Bool SupportsStage(ERenderStage stage) const;

	const ViewRenderingDataContainer&	GetData() const;
	ViewRenderingDataContainer&			GetData();

	const RenderStageEntries&	GetRenderStageEntries(ERenderStage stage) const;
	RenderStageEntries&			GetRenderStageEntries(ERenderStage stage);

private:

	lib::HashMap<SizeType, RenderStageEntries> m_stagesEntries;

	ViewRenderingDataContainer m_viewRenderingData;

	const RenderView* m_renderView;
};

} // spt::rsc