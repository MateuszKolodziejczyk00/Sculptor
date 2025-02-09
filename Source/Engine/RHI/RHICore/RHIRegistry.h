#pragma once

#include "SculptorCoreTypes.h"
#include "Memory.h"


namespace spt::rhi
{


template<typename TDataType>
class PagedArray
{
public:

	using DataType = TDataType;
	using ID = Uint32;// TODO

	PagedArray() = default;

	~PagedArray()
	{
		Deinitialize();
	}

	void Initialize(SizeType reservedSize, SizeType committedSize)
	{
		SPT_CHECK(reservedSize > 0u);

		SPT_CHECK(!m_data);

		const SizeType pageSize = mem::GetPageSize();

		//SPT_CHECK(math::Utils::)// TODO check if reservedSize is multiple of pageSize
		//SPT_CHECK(math::Utils::)// TODO check if reservedSize is multiple of pageSize

		const SizeType committedSizeAligned = math::Utils::RoundUp(committedSize, pageSize);

		m_data = mem::ReserveVirtualMemory(reservedSize);
		mem::CommitVirtualMemory(m_data, committedSizeAligned);

		m_reservedSize = reservedSize;
		m_committedSize.store(committedSizeAligned);

		SPT_CHECK(!!m_data);
	}

	void Deinitialize()
	{
		if (m_data)
		{
			const SizeType committedSize = m_committedSize.load();
			if (committedSize > 0u)
			{
				mem::DecommitVirtualMemory(m_data, committedSize);
			}

			mem::ReleaseVirtualMemory(m_data, m_reservedSize);
			m_data = nullptr;
		}
	}

	void CommitElement(ID id)
	{
		SPT_CHECK(!!m_data);

		const SizeType elementSize = sizeof(DataType);
		const SizeType elementOffset = id * elementSize;

		SPT_CHECK(elementOffset + elementSize < m_reservedSize);

		SizeType committedSize = m_committedSize.load();

		while (elementOffset + elementSize > committedSize)
		{
			if (elementOffset <= committedSize)
			{
				const SizeType pageSize = mem::GetPageSize();

				SPT_CHECK(elementSize < mem::GetPageSize()); // this would require small rework here

				// This thread will commit next page
				const SizeType newCommittedSize = committedSize + pageSize;
				mem::CommitVirtualMemory(m_data + committedSize, pageSize);

				m_committedSize.store(newCommittedSize);
			}
			else
			{
				// Just busy wait for another page to be committed.
				// Pages are committed one by one, which should be ok in most cases, but may be a bottleneck if
				// there will be a lot of threads creating new elements at the same time.

				SizeType oldCommittedSize = committedSize;
				while (oldCommittedSize == committedSize)
				{
					platf::Platform::SwitchToThread();
					committedSize = m_committedSize.load();
				}
			}
		}

		SPT_CHECK(elementOffset + elementSize <= committedSize);
	}

	void DecommitElement(ID id)
	{
		// Do nothing, element is free to be reused
	}

	DataType& operator[](ID id)
	{
		SPT_CHECK(!!m_data);

		const SizeType elementSize = sizeof(DataType);
		const SizeType elementOffset = id * elementSize;

		SPT_CHECK(elementOffset + elementSize <= m_reservedSize);

		return *reinterpret_cast<DataType*>(m_data + elementOffset);
	}

private:

	Byte* m_data = nullptr;

	SizeType m_reservedSize  = 0u;
	std::atomic<SizeType> m_committedSize = 0u;
};


template<typename... TDataTypes>
class Registry
{
public:

	using ID = Uint32;

	static constexpr ID invalidID = maxValue<ID>;

	Registry() = default;

	void Initialize();

	ID   Allocate();
	void Free(ID id);

	template<typename TDataType>
	TDataType& Get(ID id);

private:

	ID AllocateID();

	std::tuple<PagedArray<TDataTypes>...>  m_dataArrays;

	PagedArray<ID>      m_freeIDsArray;

	std::atomic<Uint32> m_freeIndicesNum = 0u;

	std::atomic<Uint32> m_commitedIDs = 0u;
};


template<typename... TDataTypes>
void Registry<TDataTypes...>::Initialize()
{
	const SizeType reservedElementsNum  = 1024u;
	const SizeType committedElementsNum = 128u;


	//std::get<PagedArray<TDataTypes>>(m_dataArrays).Initialize(reservedElementsNum, committedElementsNum)...;
}

template<typename... TDataTypes>
ID Registry<TDataTypes...>::Allocate()
{
	const ID id = AllocateID();

	std::get<PagedArray<TDataTypes>>(m_dataArrays).CommitElement(reservedElementsNum, committedElementsNum)...;

	return id;
}

template<typename... TDataTypes>
void Registry<TDataTypes...>::Free(ID id)
{
	std::get<PagedArray<TDataTypes>>(m_dataArrays).DecommitElement(reservedElementsNum, committedElementsNum)...;

	const Uint32 idx = m_freeIndicesNum.fetch_add(1u);

	m_freeIDsArray.CommitElement(idx);
	m_freeIDsArray[idx] = id;
}

template<typename... TDataTypes>
template<typename TDataType>
TDataType& Registry<TDataTypes...>::Get(ID id)
{
	return std::get<PagedArray<TDataType>>(m_dataArrays)[id];
}

template<typename... TDataTypes>
ID Registry<TDataTypes...>::AllocateID()
{
	const Uint32 freeIndicesNum = maxValue<Uint32>;
	while (freeIndicesNum > 0u && !m_freeIndicesNum.compare_exchange_weak(freeIndicesNum, freeIndicesNum - 1u)) { }

	Uint32 freeIndex = invalidID;

	if (freeIndicesNum > 0u)
	{
		freeIndex = m_freeIndices[freeIndicesNum - 1u];
		m_freeIndices[freeIndicesNum - 1u] = invalidID;
	}
	else
	{
		freeIndex = m_commitedIDs.fetch_add(1u);
	}

	SPT_CHECK(freeIndex != invalidID);
	return freeIndex;
}

} // spt::rhi