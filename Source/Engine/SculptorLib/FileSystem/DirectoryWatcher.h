#pragma once

#include "FileSystem/File.h"
#include "SculptorCoreTypes.h"
#include "Delegates/Delegate.h"


namespace spt::lib
{

struct FileModifiedPayload
{
	lib::String fileName;
	lib::Path   relativePath;
};

using FileModifiedCallback = lib::Delegate<void(const FileModifiedPayload& )>;
using FileWatcherHandle = Uint64;


struct WatchParams
{
	lib::Path            directory;
	FileModifiedCallback callback;
	Bool                 watchSubdirectories = false;
};


FileWatcherHandle StartWatchingDirectory(WatchParams params);
FileWatcherHandle StartWatchingDirectory(const lib::Path& directory, FileModifiedCallback callback);
void              StopWatchingDirectory(FileWatcherHandle handle);

} // spt::lib
