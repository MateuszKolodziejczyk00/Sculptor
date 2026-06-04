#include "DirectoryWatcher.h"
#include "Utility/String/StringUtils.h"

#ifdef SPT_PLATFORM_WINDOWS
#include <Windows.h>
#endif // SPT_PLATFORM_WINDOWS


namespace spt::lib
{

namespace priv
{
#ifdef SPT_PLATFORM_WINDOWS

struct FileWatcherContext
{
	FileModifiedCallback callback;
	HANDLE               stopEvent = INVALID_HANDLE_VALUE;
	std::thread          thread;
};


void WatcherThreadProc(FileWatcherContext* context, HANDLE hDir, Bool watchSubdirectories)
{
	alignas(FILE_NOTIFY_INFORMATION) Byte buffer[1024];

	OVERLAPPED overlapped{};
	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	HANDLE handles[] = { overlapped.hEvent, context->stopEvent };

	while (true)
	{
		if (!ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), watchSubdirectories, FILE_NOTIFY_CHANGE_LAST_WRITE, NULL, &overlapped, NULL))
		{
			break;
		}

		DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
		if (waitResult == WAIT_OBJECT_0 + 1) // context->stopEvent
		{
			CancelIo(hDir);
			break;
		}

		if (waitResult == WAIT_OBJECT_0) // overlapped.hEvent
		{
			DWORD bytesReturned = 0;
			if (GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE) && bytesReturned > 0)
			{
				FILE_NOTIFY_INFORMATION* notify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
				while (notify)
				{
					const lib::String relativePath = lib::StringUtils::ToMultibyteString({ notify->FileName, notify->FileNameLength / sizeof(WCHAR) });

					FileModifiedPayload payload;
					payload.relativePath = lib::Path(relativePath);
					payload.fileName     = lib::Path(relativePath).filename().string();
					context->callback.ExecuteIfBound(payload);

					if (notify->NextEntryOffset == 0)
					{
						break;
					}

					notify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<Byte*>(notify) + notify->NextEntryOffset);
				}
			}

			ResetEvent(overlapped.hEvent);
		}
	}
}


FileWatcherHandle StartWatchingDirectoryImpl(WatchParams params)
{
	HANDLE hDir = CreateFileW(
		params.directory.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		NULL);

	if (hDir == INVALID_HANDLE_VALUE)
	{
		return 0u;
	}

	FileWatcherContext* context = new FileWatcherContext();
	context->callback  = std::move(params.callback);
	context->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	context->thread = std::thread(WatcherThreadProc, context, hDir, params.watchSubdirectories);

	return reinterpret_cast<FileWatcherHandle>(context);
}


void StopWatchingDirectoryImpl(FileWatcherHandle handle)
{
	FileWatcherContext* context = reinterpret_cast<FileWatcherContext*>(handle);
	if (context)
	{
		SetEvent(context->stopEvent);
		if (context->thread.joinable())
		{
			context->thread.join();
		}
		CloseHandle(context->stopEvent);
		delete context;
	}
}

#endif // SPT_PLATFORM_WINDOWS
} // priv


FileWatcherHandle StartWatchingDirectory(WatchParams params)
{
	return priv::StartWatchingDirectoryImpl(std::move(params));
}

FileWatcherHandle StartWatchingDirectory(const lib::Path& directory, FileModifiedCallback callback)
{
	return StartWatchingDirectory({ directory, std::move(callback) });
}

void StopWatchingDirectory(FileWatcherHandle handle)
{
	priv::StopWatchingDirectoryImpl(handle);
}

} // spt::lib
