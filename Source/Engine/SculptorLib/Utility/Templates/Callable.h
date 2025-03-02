#pragma once

namespace spt::lib
{

template<typename TArg>
class RawCallable
{
};


template<typename TReturn, typename... TArgs>
class RawCallable<TReturn(TArgs...)>
{
public:

	using FunctionType = TReturn(TArgs...);
	using ReturnType   = TReturn;

	RawCallable() = default;

	RawCallable(FunctionType* callable)
		: m_callable(callable)
	{}

	RawCallable(const RawCallable&) = default;
	RawCallable(RawCallable&&) = default;

	RawCallable& operator=(const RawCallable&) = default;
	RawCallable& operator=(RawCallable&&) = default;

	Bool operator==(const RawCallable& other) const
	{
		return m_callable == other.m_callable;
	}

	Bool operator!=(const RawCallable& other) const
	{
		return !(*this == other);
	}

	Bool IsValid() const
	{
		return m_callable != nullptr;
	}

	ReturnType operator()(TArgs... args) const
	{
		return m_callable(args...);
	}

	FunctionType* Get() const
	{
		return m_callable;
	}

private:

	FunctionType* m_callable = nullptr;
};


template<typename TFunc>
struct Hasher<RawCallable<TFunc>>
{
    size_t operator()(const RawCallable<TFunc>& callable) const
    {
		return GetHash(callable.Get());
    }
};

} // spt::lib