#pragma once

#include "DDCMacros.h"
#include "SculptorCore.h"
#include "FileSystem/File.h"
#include "Utility/String/StringUtils.h"
#include "Backends/DDCBackend.h"
#include "Serialization.h"


namespace spt::as
{

struct DDCResourceMapping
{
	Uint64 offset   = 0u;
	Uint64 size     = idxNone<Uint64>;
	Bool   writable = false;
};


using DerivedDataName = lib::StaticArray<char, 33>;


struct DerivedDataKey
{
public:

	DerivedDataKey() = default;
	DerivedDataKey(const DerivedDataKey&) = default;
	DerivedDataKey(DerivedDataKey&&) = default;
	DerivedDataKey& operator=(const DerivedDataKey&) = default;
	DerivedDataKey& operator=(DerivedDataKey&&) = default;

	DerivedDataKey(Uint64 key0, Uint64 key1)
		: m_key{ key0, key1 }
	{
	}

	explicit DerivedDataKey(const DerivedDataName& name)
	{
		*this = name;
	}

	DerivedDataKey& operator=(const DerivedDataName& name)
	{
		lib::StringUtils::FromHexString({ name.data(), name.size() - 1u }, { reinterpret_cast<Byte*>(m_key), sizeof(Uint64) * 2 });
		return *this;
	}

	bool operator==(const DerivedDataKey& other) const
	{
		return m_key[0] == other.m_key[0] && m_key[1] == other.m_key[1];
	}

	bool operator!=(const DerivedDataKey& other) const
	{
		return !(*this == other);
	}

	Bool IsValid() const
	{
		return m_key[0] != 0u || m_key[1] != 0u;
	}

	DerivedDataName GetName() const
	{
		DerivedDataName name;
		lib::StringUtils::ToHexString(reinterpret_cast<const Byte*>(m_key), 2 * sizeof(Uint64), { name });
		name[name.size() - 1] = '\0';
		return name;
	}

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Key0", m_key[0]);
		serializer.Serialize("Key1", m_key[1]);
	}

private:

	Uint64 m_key[2] = { 0u, 0u };
};


class DDC_API DDCResourceHandle
{
public:

	DDCResourceHandle() = default;

	DDCResourceHandle(DDCResourceHandle&& rhs);
	DDCResourceHandle& operator=(DDCResourceHandle&& rhs);

	DDCResourceHandle(DerivedDataKey key, const ddc_backend::DDCInternalHandle& handle, const DDCResourceMapping& mapping);

	~DDCResourceHandle() { Release(); }

	DDCResourceHandle(const DDCResourceHandle&) = delete;
	DDCResourceHandle& operator=(const DDCResourceHandle&) = delete;

	void Initialize(DerivedDataKey key, const ddc_backend::DDCInternalHandle& handle, const DDCResourceMapping& mapping);

	void Release();

	void FlushWrites()
	{
		SPT_CHECK(IsValid());
		SPT_CHECK(m_handle.allowsWrite);
		ddc_backend::FlushWrites(m_handle);
	}

	Bool IsValid() const { return ddc_backend::IsValid(m_handle); }

	lib::Span<Byte> GetMutableSpan() const
	{
		SPT_CHECK(IsValid());
		SPT_CHECK(m_handle.allowsWrite);

		return lib::Span<Byte>(static_cast<Byte*>(m_handle.data), m_mapping.size);
	}

	lib::Span<const Byte> GetImmutableSpan() const
	{
		SPT_CHECK(IsValid());

		return lib::Span<const Byte>(static_cast<Byte*>(m_handle.data), m_mapping.size);
	}

	Byte* GetMutablePtr() const
	{
		SPT_CHECK(IsValid());
		SPT_CHECK(m_handle.allowsWrite);
		return static_cast<Byte*>(m_handle.data);
	}

	const Byte* GetImmutablePtr() const
	{
		SPT_CHECK(IsValid());
		return static_cast<const Byte*>(m_handle.data);
	}

	SizeType GetSize() const
	{
		SPT_CHECK(IsValid());
		return m_mapping.size;
	}

	DerivedDataKey GetKey() const
	{
		SPT_CHECK(IsValid());

		return m_key;
	}

private:

	ddc_backend::DDCInternalHandle m_handle;

	DDCResourceMapping m_mapping;

	DerivedDataKey m_key;
};


struct DDCParams
{
	lib::Path path;
};


class DDC_API DDC
{
public:

	DDC();

	void Initialize(const DDCParams& params);

	DerivedDataKey CreateDerivedData(const DerivedDataKey& key, lib::Span<const Byte> data) const;

	DDCResourceHandle CreateDerivedData(const DerivedDataKey& key, Uint64 size) const;

	DDCResourceHandle GetResourceHandle(const DerivedDataKey& key, const DDCResourceMapping& mapping = DDCResourceMapping()) const;

	void DeleteDerivedData(const DerivedDataKey& key) const;

	Bool DoesKeyExist(const DerivedDataKey& key) const;

	lib::Path GetDerivedDataPath(const DerivedDataKey& key) const;

private:

	DDCParams m_params;
};

} // spt::as
