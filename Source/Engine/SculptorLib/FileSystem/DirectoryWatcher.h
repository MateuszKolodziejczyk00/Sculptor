#pragma once

#include "FileSystem/File.h"
#include "SculptorCoreTypes.h"
#include "Delegates/Delegate.h"


namespace spt::lib
{

struct FileModifiedPayload
{
	lib::String fileName;
};

using FileModifiedCallback = lib::Delegate<void(const FileModifiedPayload& )>;
using FileWatcherHandle = Uint64;


FileWatcherHandle StartWatchingDirectory(const lib::Path& directory, FileModifiedCallback callback);
void              StopWatchingDirectory(FileWatcherHandle handle);

} // spt::lib
