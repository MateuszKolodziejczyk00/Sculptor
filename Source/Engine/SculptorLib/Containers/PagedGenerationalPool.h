#pragma once

#include "SculptorCoreTypes.h"

#include "Allocators/MemoryArena.h"
#include "Utility/Threading/Spinlock.h"
#include "Utility/Threading/ThreadUtils.h"
#include "Math/MathUtils.h"


namespace spt::lib
{

template<typename TTag>
struct PagedGenerationalPoolHandle
{
	Uint32 idx = 0xFFFFFFFFu;
	Uint16 generation  = 0u;

	Bool IsValid() const
	{
		return idx != 0xFFFFFFFFu;
	}

	std::strong_ordering operator<=>(const PagedGenerationalPoolHandle& other) const = default;
};


template<typename TType>
class PagedGenerationalPool
{
	struct Page
	{
		std::atomic<Uint64>          isOccpuedied    = 0u;
		std::atomic<Uint64>          isPendingDelete = 0u;
		lib::StaticArray<Uint16, 64> generation      = { 0u };

		using InstancesArray = lib::StaticArray<lib::TypeStorage<TType>, 64>;

		SPT_ALIGNAS_CACHE_LINE InstancesArray instances;
	};

public:

	using Handle = PagedGenerationalPoolHandle<TType>;

	PagedGenerationalPool(const char* memoryArenaName, Uint32 commitedNumElements = 128u, Uint32 reservedNumElements = 32u * 1024u)
		: m_pagesArena("RTSceneInstancesStorage_PagesArena", math::Utils::DivideCeil<Uint32>(commitedNumElements, 64u) * sizeof(Page), math::Utils::DivideCeil<Uint32>(reservedNumElements, 64u) * sizeof(Page))
	{
		const Uint64 initialNumPages = math::Utils::DivideCeil(commitedNumElements, 64u);

		m_pages = reinterpret_cast<Page*>(m_pagesArena.Allocate(initialNumPages * sizeof(Page), alignof(Page)));

		m_numPages            = initialNumPages;
		m_numInitializedPages = initialNumPages;

		for (Uint64 i = 0; i < m_numPages; ++i)
		{
			new (&m_pages[i]) Page();
		}
	}

	~PagedGenerationalPool()
	{
		Clear();
	}

	Uint32 GetNum() const
	{
		return m_numOccupied.load();
	}

	Handle Add(const TType& instance)
	{
		while (true)
		{
			const Uint64 rootIsAnyFree = m_rootIsAnyFree.load();
			if (rootIsAnyFree == 0ull)
			{
				return Handle{};
			}

			const Uint64 firstFreeNodeIdx = math::Utils::LowestSetBitIdx(rootIsAnyFree);

			HierarchyNode& node = m_hierarchyNodes[firstFreeNodeIdx];

			{
				lib::LockGuard lock(node.lock);

				const Uint64 nodeIsAnyFree = node.isAnyFree;
				if (nodeIsAnyFree == 0ull)
				{
					continue;
				}

				const Uint64 firstFreePageIdxInNode = math::Utils::LowestSetBitIdx(nodeIsAnyFree);
				const Uint64 pageIdx = (firstFreeNodeIdx * 64u) + firstFreePageIdxInNode;
				if (pageIdx >= m_numPages)
				{
					m_pagesArena.GrowTo((pageIdx + 1u) * sizeof(Page));

					Uint64 prevNumPages = m_numPages.load();
					while (prevNumPages < (pageIdx + 1u))
					{
						m_numPages.compare_exchange_weak(prevNumPages, pageIdx + 1u);
					}

					// This doesn't have race condition because of node lock above. only threads that allocate 2 different nodes can get here, and then correct initialization will be ensured by m_numInitializedPages atomic variable.
					Uint64 prevNumInitializedPages = m_numInitializedPages.load();
					while (prevNumInitializedPages < (pageIdx + 1u))
					{
						if (m_numInitializedPages.compare_exchange_weak(prevNumInitializedPages, pageIdx + 1u))
						{
							for (Uint64 i = prevNumInitializedPages; i < (pageIdx + 1u); ++i)
							{
								new (&m_pages[i]) Page();
							}
						}
					}
				}

				Page& page = m_pages[pageIdx];

				Uint64 pagePrevIsOccupied = page.isOccpuedied.load();

				const Uint64 firstFreeInstanceIdx = math::Utils::LowestSetBitIdx(~pagePrevIsOccupied);
				const Uint64 pageNewIsOccupied = pagePrevIsOccupied | (1ull << firstFreeInstanceIdx);

				page.instances[firstFreeInstanceIdx].Construct(instance);

				SPT_MAYBE_UNUSED
				const Uint64 preOrIsOccupied = page.isOccpuedied.fetch_or(1ull << firstFreeInstanceIdx);
				SPT_CHECK((preOrIsOccupied & (1ull << firstFreeInstanceIdx)) == 0ull);

				const Uint16 generation = ++page.generation[firstFreeInstanceIdx];

				const Bool pageHasAnyFree = (pageNewIsOccupied != ~0ull);

				if (!pageHasAnyFree)
				{
					node.isAnyFree = (node.isAnyFree & ~(1ull << firstFreePageIdxInNode));
					if (node.isAnyFree == 0ull)
					{
						m_rootIsAnyFree.fetch_and(~(1ull << firstFreeNodeIdx));
					}
				}

				m_numOccupied.fetch_add(1u);

				node.isAnyOccupied |= (1ull << firstFreePageIdxInNode);

				m_rootIsAnyOccupied.fetch_or(1ull << firstFreeNodeIdx);

				return Handle{ .idx = static_cast<Uint32>((pageIdx * 64u) + firstFreeInstanceIdx), .generation = generation };
			}
		}
	}

	Bool Delete(Handle handle)
	{
		const Uint32 pageIdx = handle.idx / 64u;
		if (pageIdx >= m_numPages)
		{
			return false;
		}

		const Uint32 instanceIdxInPage = handle.idx % 64u;

		Page& page = m_pages[pageIdx];
		if (handle.generation != page.generation[instanceIdxInPage] || (page.isOccpuedied.load() & (1ull << instanceIdxInPage)) == 0ull)
		{
			return false;
		}

		const Uint32 nodeIdx = pageIdx / 64u;
		HierarchyNode& node = m_hierarchyNodes[nodeIdx];

		Bool instanceDeleted = false;
		{
			lib::LockGuard lock(node.lock);
			const Uint64 prevPendingDelete = page.isPendingDelete.fetch_or(1ull << instanceIdxInPage);
			node.isAnyPendingDelete |= (1ull << (pageIdx % 64u));

			instanceDeleted = (prevPendingDelete & (1ull << instanceIdxInPage)) == 0ull;
		}

		if (instanceDeleted)
		{
			m_rootIsPendingDelete.fetch_or(1ull << nodeIdx);

			SPT_MAYBE_UNUSED
			const Uint32 prevOccupied = m_numOccupied.fetch_sub(1u);

			SPT_CHECK(prevOccupied > 0u);
		}

		return instanceDeleted;
	}

	void Flush()
	{
		Uint64 rootMask = m_rootIsPendingDelete.exchange(0ull);
		while (rootMask != 0ull)
		{
			const Uint64 nodeIdx = math::Utils::LowestSetBitIdx(rootMask);
			rootMask &= ~(1ull << nodeIdx);

			HierarchyNode& node = m_hierarchyNodes[nodeIdx];

			{
				lib::LockGuard lock(node.lock);

				Uint64 nodeMask = node.isAnyPendingDelete;
				while (nodeMask != 0ull)
				{
					const Uint64 pageIdxInNode = math::Utils::LowestSetBitIdx(nodeMask);
					nodeMask &= ~(1ull << pageIdxInNode);

					const Uint64 pageIdx = (nodeIdx * 64u) + pageIdxInNode;
					Page& page = m_pages[pageIdx];

					const Uint64 pagePendingDeletes = page.isPendingDelete.fetch_and(0ull);

					{
						Uint64 pageMask = pagePendingDeletes;
						while (pageMask != 0ull)
						{
							const Uint64 instanceIdxInPage = math::Utils::LowestSetBitIdx(pageMask);
							pageMask &= ~(1ull << instanceIdxInPage);

							page.instances[instanceIdxInPage].Destroy();
						}
					}

					const Uint64 prevPageIsOccupied = page.isOccpuedied.fetch_and(~pagePendingDeletes);
					const Uint64 newPageIsOccupied = prevPageIsOccupied & ~pagePendingDeletes;

					const Bool pageHasAnyOccupied = (newPageIsOccupied != 0ull);

					if (!pageHasAnyOccupied)
					{
						node.isAnyOccupied &= ~(1ull << pageIdxInNode);

						if (node.isAnyOccupied == 0ull)
						{
							m_rootIsAnyOccupied.fetch_and(~(1ull << nodeIdx));
						}
					}
					else if (prevPageIsOccupied == ~0ull)
					{
						node.isAnyFree |= (1ull << pageIdxInNode);
						m_rootIsAnyFree.fetch_or(1ull << nodeIdx);
					}

					page.isPendingDelete.fetch_and(0ull);
				}

				node.isAnyPendingDelete = 0ull;
			}
		}
	}

	TType* Get(Handle handle) const
	{
		const Uint32 pageIdx = handle.idx / 64u;
		if (pageIdx >= m_numPages)
		{
			return nullptr;
		}

		const Uint32 instanceIdxInPage = handle.idx % 64u;

		Page& page = m_pages[pageIdx];
		const Uint64 pageMask = page.isOccpuedied.load() & ~page.isPendingDelete.load();
		if (handle.generation != page.generation[instanceIdxInPage] || (pageMask & (1ull << instanceIdxInPage)) == 0ull)
		{
			return nullptr;
		}

		return page.instances[instanceIdxInPage].GetAddress();
	}

	TType& GetRef(Handle handle) const
	{
		TType* instancePtr = Get(handle);
		SPT_CHECK(instancePtr != nullptr);
		return *instancePtr;
	}

	Handle GetHandle(TType* instancePtr)
	{
		const Uint64 instanceAddr = reinterpret_cast<Uint64>(instancePtr);
		const Uint64 baseAddr     = reinterpret_cast<Uint64>(m_pages);

		const Uint64 pageIdx = (instanceAddr - baseAddr) / sizeof(Page);

		SPT_CHECK(pageIdx < m_numPages.load());

		const Page& page = m_pages[pageIdx];

		const Uint64 instanceIdxInPage = (instanceAddr - reinterpret_cast<Uint64>(page.instances.data())) / sizeof(lib::TypeStorage<TType>);

		const Uint32 instanceIdx = static_cast<Uint32>((pageIdx * 64u) + instanceIdxInPage);

		return Handle{ .idx = instanceIdx, .generation = page.generation[instanceIdxInPage] };
	}

	// Not thread safe
	template<typename TFunc>
	void ForEach(TFunc func) const
	{
		Uint64 rootMask = m_rootIsAnyOccupied.load();
		while (rootMask != 0ull)
		{
			const Uint64 nodeIdx = math::Utils::LowestSetBitIdx(rootMask);
			rootMask &= ~(1ull << nodeIdx);

			const HierarchyNode& node = m_hierarchyNodes[nodeIdx];
			Uint64 nodeMask = node.isAnyOccupied;

			while (nodeMask != 0ull)
			{
				const Uint64 pageIdxInNode = math::Utils::LowestSetBitIdx(nodeMask);
				nodeMask &= ~(1ull << pageIdxInNode);

				const Uint64 pageIdx = (nodeIdx * 64u) + pageIdxInNode;
				Page& page = m_pages[pageIdx];

				Uint64 pageMask = page.isOccpuedied.load() & ~page.isPendingDelete.load();
				while (pageMask != 0ull)
				{
					const Uint64 instanceIdxInPage = math::Utils::LowestSetBitIdx(pageMask);
					pageMask &= ~(1ull << instanceIdxInPage);

					const Handle handle{ .idx = static_cast<Uint32>((pageIdx * 64u) + instanceIdxInPage), .generation = page.generation[instanceIdxInPage] };

					func(handle, page.instances[instanceIdxInPage].Get());
				}
			}
		}
	}

	// Not thread safe
	void Clear()
	{
		Uint64 rootMask = m_rootIsAnyOccupied.load();
		while (rootMask != 0ull)
		{
			const Uint64 nodeIdx = math::Utils::LowestSetBitIdx(rootMask);
			rootMask &= ~(1ull << nodeIdx);

			HierarchyNode& node = m_hierarchyNodes[nodeIdx];

			Uint64 nodeMask = node.isAnyOccupied;
			while (nodeMask != 0ull)
			{
				const Uint64 pageIdxInNode = math::Utils::LowestSetBitIdx(nodeMask);
				nodeMask &= ~(1ull << pageIdxInNode);

				const Uint64 pageIdx = (nodeIdx * 64u) + pageIdxInNode;
				Page& page = m_pages[pageIdx];

				Uint64 pageMask = page.isOccpuedied.load();
				while (pageMask != 0ull)
				{
					const Uint64 instanceIdxInPage = math::Utils::LowestSetBitIdx(pageMask);
					pageMask &= ~(1ull << instanceIdxInPage);

					page.instances[instanceIdxInPage].Destroy();
				}
				page.isOccpuedied.store(0ull);
				page.isPendingDelete.store(0ull);
			}

			node.isAnyOccupied      = 0ull;
			node.isAnyFree          = ~0ull;
			node.isAnyPendingDelete = 0ull;
		}

		m_rootIsAnyOccupied.store(0ull);
		m_rootIsAnyFree.store(~0ull);

		m_numOccupied.store(0u);
	}

private:

	std::atomic<Uint64> m_rootIsAnyOccupied   = 0ull;
	std::atomic<Uint64> m_rootIsAnyFree       = ~0ull;
	std::atomic<Uint64> m_rootIsPendingDelete = 0ull;

	struct HierarchyNode
	{
		Uint64 isAnyOccupied      = 0ull;
		Uint64 isAnyFree          = ~0ull;
		Uint64 isAnyPendingDelete = 0ull;
		lib::Spinlock lock;
	};

	lib::StaticArray<HierarchyNode, 64> m_hierarchyNodes;

	lib::ThreadSafeMemoryArena m_pagesArena;

	std::atomic<Uint32> m_numOccupied = 0u;

	Page* m_pages = nullptr;
	std::atomic<Uint64> m_numPages = 0;
	std::atomic<Uint64> m_numInitializedPages = 0;
};

} // spt::lib


namespace std
{
template<typename T>
struct hash<spt::lib::PagedGenerationalPoolHandle<T>>
{
	size_t operator()(const spt::lib::PagedGenerationalPoolHandle<T>& handle) const
	{
		const spt::Uint64 idxHash = handle.idx | (static_cast<spt::Uint64>(handle.generation) << 32u);
		return std::hash<spt::Uint64>{}(idxHash);
	}
};
} // std
