#pragma once

#include "SculptorAliases.h"
#include "Assertions/Assertions.h"
#include "Utility/String/String.h"


namespace spt::lib
{

namespace impl
{

template<typename TChar, SizeType maxLength>
class InlineString
{
public:

	using CharType        = TChar;
	using ViewType        = std::basic_string_view<TChar>;
	using TraitsType      = std::char_traits<TChar>;
	using value_type      = TChar;
	using traits_type     = TraitsType;
	using reference       = TChar&;
	using const_reference = const TChar&;
	using pointer         = TChar*;
	using const_pointer   = const TChar*;
	using iterator        = TChar*;
	using const_iterator  = const TChar*;
	using size_type       = SizeType;
	using difference_type = std::ptrdiff_t;

	static constexpr SizeType npos = ViewType::npos;

	constexpr InlineString() = default;

	constexpr InlineString(const TChar* str)
	{
		SPT_CHECK(str != nullptr);
		Append(ViewType(str));
	}

	constexpr InlineString(const TChar* str, SizeType length)
	{
		Append(ViewType(str, length));
	}

	constexpr InlineString(ViewType view)
	{
		Append(view);
	}

	template<SizeType otherLength>
	requires (otherLength != maxLength)
	constexpr InlineString(const InlineString<TChar, otherLength>& other)
	{
		Append(other.GetView());
	}

	constexpr InlineString(const InlineString& other) = default;
	constexpr InlineString& operator=(const InlineString& other) = default;

	constexpr InlineString& operator=(const TChar* str)
	{
		SPT_CHECK(str != nullptr);

		Assign(ViewType(str));
		return *this;
	}

	constexpr InlineString& operator=(ViewType view)
	{
		Assign(view);
		return *this;
	}

	template<SizeType otherLength>
	requires (otherLength != maxLength)
	constexpr InlineString& operator=(const InlineString<TChar, otherLength>& other)
	{
		Assign(other.GetView());
		return *this;
	}

	constexpr void Assign(ViewType view)
	{
		SPT_CHECK(view.size() <= maxLength);

		TraitsType::move(m_data, view.data(), view.size());
		SetLength(view.size());
	}

	constexpr SizeType GetLength() const { return m_length; }
	constexpr SizeType size() const { return m_length; }
	constexpr SizeType length() const { return m_length; }

	constexpr Bool IsEmpty() const { return m_length == 0u; }
	constexpr Bool empty() const { return IsEmpty(); }

	constexpr Bool IsFull() const { return m_length == maxLength; }

	constexpr SizeType GetRemainingCapacity() const { return maxLength - m_length; }

	static constexpr SizeType GetCapacity() { return maxLength; }
	static constexpr SizeType capacity() { return maxLength; }
	static constexpr SizeType max_size() { return maxLength; }

	constexpr reference operator[](SizeType index)
	{
		SPT_CHECK(index < m_length);
		return m_data[index];
	}

	constexpr const_reference operator[](SizeType index) const
	{
		SPT_CHECK(index < m_length);
		return m_data[index];
	}

	constexpr reference Front()
	{
		SPT_CHECK(!IsEmpty());
		return m_data[0];
	}

	constexpr const_reference Front() const
	{
		SPT_CHECK(!IsEmpty());
		return m_data[0];
	}

	constexpr reference Back()
	{
		SPT_CHECK(!IsEmpty());
		return m_data[m_length - 1u];
	}

	constexpr const_reference Back() const
	{
		SPT_CHECK(!IsEmpty());
		return m_data[m_length - 1u];
	}

	constexpr pointer GetData() { return m_data; }
	constexpr const_pointer GetData() const { return m_data; }
	constexpr pointer data() { return m_data; }
	constexpr const_pointer data() const { return m_data; }

	constexpr const_pointer GetCString() const { return m_data; }
	constexpr const_pointer c_str() const { return m_data; }

	constexpr iterator begin() { return m_data; }
	constexpr const_iterator begin() const { return m_data; }
	constexpr const_iterator cbegin() const { return m_data; }

	constexpr iterator end() { return m_data + m_length; }
	constexpr const_iterator end() const { return m_data + m_length; }
	constexpr const_iterator cend() const { return m_data + m_length; }

	constexpr void PushBack(TChar character)
	{
		SPT_CHECK(!IsFull());

		m_data[m_length] = character;
		SetLength(m_length + 1u);
	}

	constexpr void PopBack()
	{
		SPT_CHECK(!IsEmpty());

		SetLength(m_length - 1u);
	}

	constexpr void Append(ViewType view)
	{
		SPT_CHECK(view.size() <= GetRemainingCapacity());

		TraitsType::move(m_data + m_length, view.data(), view.size());
		SetLength(m_length + view.size());
	}

	// Appends only if the whole view fits. Otherwise the string is left unchanged.
	constexpr Bool TryAppend(ViewType view)
	{
		if (view.size() > GetRemainingCapacity())
		{
			return false;
		}

		Append(view);
		return true;
	}

	// Returns false if the view had to be truncated.
	constexpr Bool AppendTruncated(ViewType view)
	{
		const SizeType appendedLength = view.size() < GetRemainingCapacity() ? view.size() : GetRemainingCapacity();
		Append(view.substr(0u, appendedLength));
		return appendedLength == view.size();
	}

	constexpr InlineString& operator+=(TChar character)
	{
		PushBack(character);
		return *this;
	}

	constexpr InlineString& operator+=(ViewType view)
	{
		Append(view);
		return *this;
	}

	constexpr InlineString& operator+=(const TChar* str)
	{
		SPT_CHECK(str != nullptr);

		Append(ViewType(str));
		return *this;
	}

	template<SizeType otherLength>
	constexpr InlineString& operator+=(const InlineString<TChar, otherLength>& other)
	{
		Append(other.GetView());
		return *this;
	}

	constexpr void Clear()
	{
		SetLength(0u);
	}

	constexpr void Resize(SizeType newLength, TChar fill = TChar())
	{
		SPT_CHECK(newLength <= maxLength);

		if (newLength > m_length)
		{
			TraitsType::assign(m_data + m_length, newLength - m_length, fill);
		}

		SetLength(newLength);
	}

	// New characters are left as they are in the buffer, for writing through GetData().
	constexpr void ResizeUninitialized(SizeType newLength)
	{
		SPT_CHECK(newLength <= maxLength);

		SetLength(newLength);
	}

	constexpr void Insert(SizeType index, ViewType view)
	{
		SPT_CHECK(index <= m_length);
		SPT_CHECK(view.size() <= GetRemainingCapacity());

		TraitsType::move(m_data + index + view.size(), m_data + index, m_length - index);
		TraitsType::copy(m_data + index, view.data(), view.size());
		SetLength(m_length + view.size());
	}

	constexpr void Insert(SizeType index, TChar character)
	{
		SPT_CHECK(index <= m_length);
		SPT_CHECK(!IsFull());

		TraitsType::move(m_data + index + 1u, m_data + index, m_length - index);
		m_data[index] = character;
		SetLength(m_length + 1u);
	}

	constexpr void Erase(SizeType index, SizeType count = npos)
	{
		SPT_CHECK(index <= m_length);

		const SizeType tailLength  = m_length - index;
		const SizeType erasedCount = count < tailLength ? count : tailLength;
		const SizeType tailStart   = index + erasedCount;

		TraitsType::move(m_data + index, m_data + tailStart, m_length - tailStart);
		SetLength(m_length - erasedCount);
	}

	constexpr ViewType GetView() const { return ViewType(m_data, m_length); }

	constexpr operator ViewType() const { return GetView(); }

	constexpr ViewType SubView(SizeType position, SizeType count = npos) const
	{
		SPT_CHECK(position <= m_length);
		return GetView().substr(position, count);
	}

	std::basic_string<TChar> ToString() const
	{
		return std::basic_string<TChar>(GetView());
	}

	constexpr SizeType Find(ViewType view, SizeType position = 0u) const { return GetView().find(view, position); }
	constexpr SizeType Find(TChar character, SizeType position = 0u) const { return GetView().find(character, position); }

	constexpr SizeType RFind(ViewType view, SizeType position = npos) const { return GetView().rfind(view, position); }
	constexpr SizeType RFind(TChar character, SizeType position = npos) const { return GetView().rfind(character, position); }

	constexpr Bool StartsWith(ViewType view) const { return GetView().starts_with(view); }
	constexpr Bool StartsWith(TChar character) const { return GetView().starts_with(character); }

	constexpr Bool EndsWith(ViewType view) const { return GetView().ends_with(view); }
	constexpr Bool EndsWith(TChar character) const { return GetView().ends_with(character); }

	constexpr Bool Contains(ViewType view) const { return Find(view) != npos; }
	constexpr Bool Contains(TChar character) const { return Find(character) != npos; }

	template<SizeType otherLength>
	constexpr Bool operator==(const InlineString<TChar, otherLength>& rhs) const { return GetView() == rhs.GetView(); }
	constexpr Bool operator==(ViewType rhs) const { return GetView() == rhs; }
	constexpr Bool operator==(const TChar* rhs) const { return GetView() == ViewType(rhs); }

private:

	constexpr void SetLength(SizeType newLength)
	{
		m_length = newLength;
		m_data[m_length] = TChar();
	}

	SizeType m_length = 0u;
	TChar    m_data[maxLength + 1u] = {};
};

} // impl


template<SizeType maxLength>
using InlineString = impl::InlineString<char, maxLength>;

template<SizeType maxLength>
using WInlineString = impl::InlineString<wchar_t, maxLength>;

} // spt::lib
