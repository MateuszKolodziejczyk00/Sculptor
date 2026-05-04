#pragma once

#include "SculptorAliases.h"
#include "DynamicArray.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <utility>


namespace spt::lib
{

template<typename TKeyType, typename TValueType, typename TKeyCompare = std::less<TKeyType>, typename TAllocator = std::allocator<std::pair<TKeyType, TValueType>>>
class FlatMap
{
public:

	using key_type        = TKeyType;
	using mapped_type     = TValueType;
	using value_type      = std::pair<TKeyType, TValueType>;
	using allocator_type  = TAllocator;
	using compare_type    = TKeyCompare;
	using size_type       = typename DynamicArray<value_type, allocator_type>::size_type;
	using difference_type = typename DynamicArray<value_type, allocator_type>::difference_type;
	using reference       = value_type&;
	using const_reference = const value_type&;
	using pointer         = typename DynamicArray<value_type, allocator_type>::pointer;
	using const_pointer   = typename DynamicArray<value_type, allocator_type>::const_pointer;
	using iterator        = typename DynamicArray<value_type, allocator_type>::iterator;
	using const_iterator  = typename DynamicArray<value_type, allocator_type>::const_iterator;

	FlatMap() = default;

	explicit FlatMap(const compare_type& compare, const allocator_type& allocator = allocator_type())
		: m_storage(allocator)
		, m_compare(compare)
	{
	}

	explicit FlatMap(const allocator_type& allocator)
		: m_storage(allocator)
	{
	}

	FlatMap(std::initializer_list<value_type> initializerList, const compare_type& compare = compare_type(), const allocator_type& allocator = allocator_type())
		: m_storage(allocator)
		, m_compare(compare)
	{
		Reserve(static_cast<size_type>(initializerList.size()));

		for (const value_type& value : initializerList)
		{
			Insert(value);
		}
	}

	size_type GetSize() const { return m_storage.size(); }
	size_type size() const { return m_storage.size(); }

	Bool IsEmpty() const { return m_storage.empty(); }
	Bool empty() const { return m_storage.empty(); }

	size_type GetCapacity() const { return m_storage.capacity(); }
	size_type capacity() const { return m_storage.capacity(); }

	void Reserve(size_type newCapacity)
	{
		m_storage.reserve(newCapacity);
	}

	void reserve(size_type newCapacity)
	{
		Reserve(newCapacity);
	}

	void Clear()
	{
		m_storage.clear();
	}

	void clear()
	{
		Clear();
	}

	pointer GetData() { return m_storage.data(); }
	const_pointer GetData() const { return m_storage.data(); }
	pointer data() { return m_storage.data(); }
	const_pointer data() const { return m_storage.data(); }

	iterator begin() { return m_storage.begin(); }
	const_iterator begin() const { return m_storage.begin(); }
	const_iterator cbegin() const { return m_storage.cbegin(); }

	iterator end() { return m_storage.end(); }
	const_iterator end() const { return m_storage.end(); }
	const_iterator cend() const { return m_storage.cend(); }

	compare_type key_comp() const
	{
		return m_compare;
	}

	iterator LowerBound(const key_type& key)
	{
		return std::lower_bound(begin(), end(), key, [this](const value_type& value, const key_type& searchedKey)
		{
			return m_compare(value.first, searchedKey);
		});
	}

	const_iterator LowerBound(const key_type& key) const
	{
		return std::lower_bound(begin(), end(), key, [this](const value_type& value, const key_type& searchedKey)
		{
			return m_compare(value.first, searchedKey);
		});
	}

	iterator lower_bound(const key_type& key)
	{
		return LowerBound(key);
	}

	const_iterator lower_bound(const key_type& key) const
	{
		return LowerBound(key);
	}

	iterator Find(const key_type& key)
	{
		iterator it = LowerBound(key);
		return IsMatchingKey(it, key) ? it : end();
	}

	const_iterator Find(const key_type& key) const
	{
		const_iterator it = LowerBound(key);
		return IsMatchingKey(it, key) ? it : end();
	}

	iterator find(const key_type& key)
	{
		return Find(key);
	}

	const_iterator find(const key_type& key) const
	{
		return Find(key);
	}

	Bool Contains(const key_type& key) const
	{
		return Find(key) != end();
	}

	Bool contains(const key_type& key) const
	{
		return Contains(key);
	}

	mapped_type& At(const key_type& key)
	{
		iterator it = Find(key);
		SPT_CHECK(it != end());
		return it->second;
	}

	const mapped_type& At(const key_type& key) const
	{
		const_iterator it = Find(key);
		SPT_CHECK(it != end());
		return it->second;
	}

	mapped_type& at(const key_type& key)
	{
		return At(key);
	}

	const mapped_type& at(const key_type& key) const
	{
		return At(key);
	}

	mapped_type& Get(const key_type& key)
	{
		return At(key);
	}

	const mapped_type& Get(const key_type& key) const
	{
		return At(key);
	}

	mapped_type* TryGet(const key_type& key)
	{
		iterator it = Find(key);
		return it != end() ? &it->second : nullptr;
	}

	const mapped_type* TryGet(const key_type& key) const
	{
		const_iterator it = Find(key);
		return it != end() ? &it->second : nullptr;
	}

	mapped_type& operator[](const key_type& key)
	{
		return TryEmplace(key).first->second;
	}

	mapped_type& operator[](key_type&& key)
	{
		return TryEmplace(std::move(key)).first->second;
	}

	std::pair<iterator, Bool> Insert(const value_type& value)
	{
		iterator it = LowerBound(value.first);
		if (IsMatchingKey(it, value.first))
		{
			return { it, false };
		}

		it = m_storage.insert(it, value);
		return { it, true };
	}

	std::pair<iterator, Bool> Insert(value_type&& value)
	{
		iterator it = LowerBound(value.first);
		if (IsMatchingKey(it, value.first))
		{
			return { it, false };
		}

		it = m_storage.insert(it, std::move(value));
		return { it, true };
	}

	std::pair<iterator, Bool> insert(const value_type& value)
	{
		return Insert(value);
	}

	std::pair<iterator, Bool> insert(value_type&& value)
	{
		return Insert(std::move(value));
	}

	template<typename... TArgs>
	std::pair<iterator, Bool> Emplace(TArgs&&... args)
	{
		return Insert(value_type(std::forward<TArgs>(args)...));
	}

	template<typename... TArgs>
	std::pair<iterator, Bool> emplace(TArgs&&... args)
	{
		return Emplace(std::forward<TArgs>(args)...);
	}

	template<typename... TArgs>
	std::pair<iterator, Bool> TryEmplace(const key_type& key, TArgs&&... args)
	{
		iterator it = LowerBound(key);
		if (IsMatchingKey(it, key))
		{
			return { it, false };
		}

		it = m_storage.emplace(it, key, mapped_type(std::forward<TArgs>(args)...));
		return { it, true };
	}

	template<typename... TArgs>
	std::pair<iterator, Bool> TryEmplace(key_type&& key, TArgs&&... args)
	{
		iterator it = LowerBound(key);
		if (IsMatchingKey(it, key))
		{
			return { it, false };
		}

		it = m_storage.emplace(it, std::move(key), mapped_type(std::forward<TArgs>(args)...));
		return { it, true };
	}

	template<typename... TArgs>
	std::pair<iterator, Bool> try_emplace(const key_type& key, TArgs&&... args)
	{
		return TryEmplace(key, std::forward<TArgs>(args)...);
	}

	template<typename... TArgs>
	std::pair<iterator, Bool> try_emplace(key_type&& key, TArgs&&... args)
	{
		return TryEmplace(std::move(key), std::forward<TArgs>(args)...);
	}

	std::pair<iterator, Bool> InsertOrAssign(const key_type& key, mapped_type value)
	{
		iterator it = LowerBound(key);
		if (IsMatchingKey(it, key))
		{
			it->second = std::move(value);
			return { it, false };
		}

		it = m_storage.emplace(it, key, std::move(value));
		return { it, true };
	}

	std::pair<iterator, Bool> InsertOrAssign(key_type&& key, mapped_type value)
	{
		iterator it = LowerBound(key);
		if (IsMatchingKey(it, key))
		{
			it->second = std::move(value);
			return { it, false };
		}

		it = m_storage.emplace(it, std::move(key), std::move(value));
		return { it, true };
	}

	std::pair<iterator, Bool> insert_or_assign(const key_type& key, mapped_type value)
	{
		return InsertOrAssign(key, std::move(value));
	}

	std::pair<iterator, Bool> insert_or_assign(key_type&& key, mapped_type value)
	{
		return InsertOrAssign(std::move(key), std::move(value));
	}

	iterator Erase(const_iterator position)
	{
		return m_storage.erase(position);
	}

	iterator erase(const_iterator position)
	{
		return Erase(position);
	}

	size_type Erase(const key_type& key)
	{
		iterator it = Find(key);
		if (it == end())
		{
			return 0u;
		}

		m_storage.erase(it);
		return 1u;
	}

	size_type erase(const key_type& key)
	{
		return Erase(key);
	}

private:

	Bool AreKeysEqual(const key_type& lhs, const key_type& rhs) const
	{
		return !m_compare(lhs, rhs) && !m_compare(rhs, lhs);
	}

	Bool IsMatchingKey(iterator it, const key_type& key) const
	{
		return it != m_storage.end() && AreKeysEqual(it->first, key);
	}

	Bool IsMatchingKey(const_iterator it, const key_type& key) const
	{
		return it != m_storage.cend() && AreKeysEqual(it->first, key);
	}

	DynamicArray<value_type, allocator_type> m_storage;
	[[no_unique_address]] compare_type       m_compare;
};

} // spt::lib
