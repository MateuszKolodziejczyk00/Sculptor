#include "Backends/DDCBackend.h"
#include "windows.h"

#pragma optimize("", off)
namespace spt::as::ddc_backend
{

SPT_DEFINE_LOG_CATEGORY(DDCBackend, true);

struct WindowsHandle
{
	HANDLE hFile    = NULL;
	HANDLE hMapping = NULL;
};

SPT_STATIC_CHECK_MSG(sizeof(WindowsHandle) <= sizeof(OpaqueHandle), "Opaque handle size is too small to store WindowsHandle");


WindowsHandle& GetWindowsOpaqueHandle(DDCInternalHandle& handle)
{
	return *reinterpret_cast<WindowsHandle*>(&handle.opaque);
}

DDCInternalHandle CreateInternalHandleForWriting(const lib::Path& path, const DDCOpenParams& params)
{
	DDCInternalHandle outHandle;

	SPT_CHECK(params.writable);

	const HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return outHandle;
	}

	const HANDLE hMapping = CreateFileMappingW(hFile, NULL, PAGE_READWRITE, 0, static_cast<DWORD>(params.size), NULL);
	if (hMapping == NULL)
	{
		CloseHandle(hFile);
		return outHandle;
	}

	WindowsHandle& windowsHandle = GetWindowsOpaqueHandle(outHandle);
	windowsHandle.hFile    = hFile;
	windowsHandle.hMapping = hMapping;

	outHandle.data = static_cast<Byte*>(MapViewOfFile(windowsHandle.hMapping, FILE_MAP_ALL_ACCESS, 0, static_cast<DWORD>(params.offset), static_cast<DWORD>(params.size)));
	outHandle.allowsWrite = true;

	return outHandle;
}

std::pair<DDCInternalHandle, SizeType> OpenInternalHandle(const lib::Path& path, const DDCOpenParams& params)
{
	DDCInternalHandle outHandle;

	DWORD access = GENERIC_READ;
	if (params.writable)
	{
		access |= GENERIC_WRITE;
	}

	const HANDLE hFile = CreateFileW(path.c_str(), access, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		SPT_LOG_ERROR(DDCBackend, "Failed To open Derived Data file. Error {}", GetLastError());

		return { outHandle, 0u };
	}

	LARGE_INTEGER size = {};
	if (params.size == idxNone<Uint64>)
	{
		if (!GetFileSizeEx(hFile, &size))
		{
			CloseHandle(hFile);
			return { outHandle, 0u };
		}

		size.QuadPart -= params.offset;
	}
	else
	{
		size.QuadPart = static_cast<LONGLONG>(params.size);
	}

	const HANDLE hMapping = CreateFileMappingW(hFile, NULL, params.writable ? PAGE_READWRITE : PAGE_READONLY, size.HighPart, size.LowPart, NULL);
	if (hMapping == NULL)
	{
		CloseHandle(hFile);
		return { outHandle, 0u };
	}

	WindowsHandle& windowsHandle = GetWindowsOpaqueHandle(outHandle);
	windowsHandle.hFile    = hFile;
	windowsHandle.hMapping = hMapping;

	outHandle.data = static_cast<Byte*>(MapViewOfFile(windowsHandle.hMapping, params.writable ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ, 0, 0, size.QuadPart));
	outHandle.allowsWrite = params.writable;

	return { outHandle, size.QuadPart };
}

void FlushWrites(DDCInternalHandle handle)
{
	SPT_CHECK(handle.data != nullptr);
	FlushViewOfFile(handle.data, 0);
}

void CloseInternalHandle(DDCInternalHandle handle)
{
	SPT_CHECK(handle.data != nullptr);

	FlushWrites(handle);

	WindowsHandle& windowsHandle = GetWindowsOpaqueHandle(handle);
	if (windowsHandle.hMapping != NULL)
	{
		UnmapViewOfFile(handle.data);
		CloseHandle(windowsHandle.hMapping);

		windowsHandle.hMapping = NULL;
	}

	if (windowsHandle.hFile != NULL)
	{
		CloseHandle(windowsHandle.hFile);

		windowsHandle.hFile = NULL;
	}

	handle.data = nullptr;
	handle.allowsWrite = false;
}

} // spt::as::ddc_backend
