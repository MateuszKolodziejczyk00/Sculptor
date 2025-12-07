#include "DDC.h"
#include "Utility/Random.h"


namespace spt::as
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDCResourceHandle =============================================================================

DDCResourceHandle::DDCResourceHandle(DDCResourceHandle&& rhs)
{
	m_handle  = std::move(rhs.m_handle);
	m_mapping = rhs.m_mapping;
	m_key     = rhs.m_key;

	rhs.m_handle  = {};
	rhs.m_mapping = {};
	rhs.m_key     = {};
}

DDCResourceHandle& DDCResourceHandle::operator=(DDCResourceHandle&& rhs)
{
	if (this != &rhs)
	{
		Release();

		m_handle  = std::move(rhs.m_handle);
		m_mapping = rhs.m_mapping;
		m_key     = rhs.m_key;

		rhs.m_handle  = {};
		rhs.m_mapping = {};
		rhs.m_key     = {};
	}

	return *this;
}

DDCResourceHandle::DDCResourceHandle(DerivedDataKey key, const ddc_backend::DDCInternalHandle& handle, const DDCResourceMapping& mapping)
{
	Initialize(key, handle, mapping);
}

void DDCResourceHandle::Initialize(DerivedDataKey key, const ddc_backend::DDCInternalHandle& handle, const DDCResourceMapping& mapping)
{
	SPT_CHECK(key.IsValid());

	Release();

	SPT_CHECK(ddc_backend::IsValid(handle));
	SPT_CHECK(mapping.size > 0u);

	m_handle  = handle;
	m_mapping = mapping;
	m_key     = key;

	SPT_CHECK(IsValid());
}

void DDCResourceHandle::Release()
{
	if (ddc_backend::IsValid(m_handle))
	{
		ddc_backend::CloseInternalHandle(m_handle);

		m_handle  = {};
		m_mapping = {};
		m_key     = {};
	}

	SPT_CHECK(!IsValid());
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDC ===========================================================================================

DDC::DDC()
{
}

void DDC::Initialize(const DDCParams& params)
{
	m_params = params;

	if (!std::filesystem::exists(params.path))
	{
		std::filesystem::create_directories(params.path);
	}
}

DerivedDataKey DDC::CreateDerivedData(const DerivedDataKey& key, lib::Span<const Byte> data) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(data.size() > 0u);
	SPT_CHECK(key.IsValid());

	const lib::Path derivedDataPath = GetDerivedDataPath(key);

	ddc_backend::DDCInternalHandle handle = ddc_backend::CreateInternalHandleForWriting(derivedDataPath, { .size = data.size(), .writable = true });

	SPT_CHECK(ddc_backend::IsValid(handle));
	SPT_CHECK(handle.allowsWrite);

	std::memcpy(handle.data, data.data(), data.size());

	ddc_backend::CloseInternalHandle(handle);

	return key;
}

DDCResourceHandle DDC::CreateDerivedData(const DerivedDataKey& key, Uint64 size) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(size > 0u);
	SPT_CHECK(key.IsValid());

	const lib::Path derivedDataPath = GetDerivedDataPath(key);

	ddc_backend::DDCInternalHandle handle = ddc_backend::CreateInternalHandleForWriting(derivedDataPath, { .size = size, .writable = true });

	SPT_CHECK(ddc_backend::IsValid(handle));
	SPT_CHECK(handle.allowsWrite);

	return DDCResourceHandle(key, handle, { .offset = 0u, .size = size });
}

DDCResourceHandle DDC::GetResourceHandle(const DerivedDataKey& key, const DDCResourceMapping& mapping /*= DDCResourceMapping()*/) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(key.IsValid());
	SPT_CHECK(mapping.size > 0u);

	const lib::Path derivedDataPath = GetDerivedDataPath(key);
	const auto [handle, actualSize] = ddc_backend::OpenInternalHandle(derivedDataPath, { .offset = mapping.offset, .size = mapping.size, .writable = mapping.writable });

	SPT_CHECK(ddc_backend::IsValid(handle));
	SPT_CHECK(handle.allowsWrite == mapping.writable);

	return DDCResourceHandle(key, handle, DDCResourceMapping{ .offset = mapping.offset, .size = actualSize });
}

void DDC::DeleteDerivedData(const DerivedDataKey& key) const
{
	SPT_PROFILER_FUNCTION();
	SPT_CHECK(key.IsValid());

	const lib::Path derivedDataPath = GetDerivedDataPath(key);

	if (std::filesystem::exists(derivedDataPath))
	{
		std::filesystem::remove(derivedDataPath);
	}
}

Bool DDC::DoesKeyExist(const DerivedDataKey& key) const
{
	const lib::Path derivedDataPath = GetDerivedDataPath(key);
	return std::filesystem::exists(derivedDataPath);
}

lib::Path DDC::GetDerivedDataPath(const DerivedDataKey& key) const
{
	return m_params.path / key.GetName().data();
}

} // spt::as

