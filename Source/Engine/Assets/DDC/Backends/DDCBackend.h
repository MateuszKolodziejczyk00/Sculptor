#pragma once

#include "SculptorCoreTypes.h"
#include "FileSystem/File.h"


namespace spt::as::ddc_backend
{

constexpr Uint64 opaqueSize = sizeof(Uint64) * 2u;

using OpaqueHandle = lib::StaticArray<Uint8, opaqueSize>;


struct DDCInternalHandle
{
	OpaqueHandle opaque = {};
	Byte*        data   = nullptr;

	Uint8 allowsWrite : 1;
};


struct DDCOpenParams
{
	Uint64 offset;
	Uint64 size;
};


inline Bool IsValid(const DDCInternalHandle& handle) { return handle.data != nullptr; }

DDCInternalHandle CreateInternalHandleForWriting(const lib::Path& path, const DDCOpenParams& params);

std::pair<DDCInternalHandle, SizeType> OpenInternalHandle(const lib::Path& path, const DDCOpenParams& params);

void CloseInternalHandle(DDCInternalHandle handle);

} // spt::as::ddc_backend