#include "File.h"

#include <filesystem>
#include <fstream>

namespace spt::lib
{

namespace priv
{

static auto GetOpenMode(EFileOpenFlags flags)
{
	std::ios_base::openmode openMode{};

	if (lib::HasAnyFlag(flags, EFileOpenFlags::DiscardContent))
	{
		lib::AddFlag(openMode, std::ios::trunc);
	}
	if (lib::HasAnyFlag(flags, EFileOpenFlags::StartAtTheEnd))
	{
		lib::AddFlag(openMode, std::ios::ate);
	}
	if (lib::HasAnyFlag(flags, EFileOpenFlags::Binary))
	{
		lib::AddFlag(openMode, std::ios::binary);
	}

	return openMode;
}

} // priv

std::ifstream File::OpenInputStream(const lib::String& path, EFileOpenFlags openFlags /*= EFileOpenFlags::None*/)
{
	std::ifstream inputStream(path, priv::GetOpenMode(openFlags));
	return inputStream;
}

std::ofstream File::OpenOutputStream(const lib::String& path, EFileOpenFlags openFlags /*= EFileOpenFlags::None*/)
{
	std::ofstream outputStream(path, priv::GetOpenMode(openFlags));
	
	if (!outputStream.is_open() && lib::HasAnyFlag(openFlags, EFileOpenFlags::ForceCreate))
	{
		const SizeType directoryPathEnd = path.find_last_of("/");
		if (directoryPathEnd != lib::String::npos)
		{
			const lib::String directoryPath(std::cbegin(path), std::cbegin(path) + directoryPathEnd);
			std::filesystem::create_directories(directoryPath);
			outputStream.open(path);
		}
	}

	SPT_CHECK(!lib::HasAnyFlag(openFlags, EFileOpenFlags::ForceCreate) || !outputStream.bad());

	return outputStream;
}

Bool File::Exists(const lib::StringView& path)
{
	return std::filesystem::exists(path);
}

lib::String File::DiscardExtension(const lib::String& file)
{
	const SizeType extensionBeginPosition = file.find_last_of('.');
	return extensionBeginPosition != lib::String::npos
		? lib::String(std::cbegin(file), std::cbegin(file) + extensionBeginPosition)
		: file;
}

lib::StringView File::GetExtension(const lib::StringView& file)
{
	const SizeType extensionBeginPosition = file.find_last_of('.');
	return extensionBeginPosition != lib::String::npos
		? lib::StringView(std::cbegin(file) + extensionBeginPosition + 1, std::cend(file))
		: lib::StringView();
}

lib::String File::Utils::CreateFileNameFromTime(lib::StringView extension /*= {}*/)
{
	const auto now       = std::chrono::system_clock::now();
	const auto nowTime   = std::chrono::system_clock::to_time_t(now);

	struct tm buf;
	localtime_s(&buf, &nowTime);

	char buffer[256];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &buf);

	lib::String result = buffer;

	if (!extension.empty())
	{
		result += '.';
		result += extension;
	}

	return result;
}

} // spt::lib
