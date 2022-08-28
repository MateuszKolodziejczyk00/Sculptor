#include "File.h"

#include <filesystem>
#include <fstream>

namespace spt::lib
{

namespace priv
{

static auto GetOpenMode(EFileOpenFlags flags)
{
	using FlagsType = std::decay_t<decltype(std::ios::trunc)>;
	FlagsType openMode{};

	if (lib::HasAnyFlag(flags, EFileOpenFlags::DiscardContent))
	{
		openMode = static_cast<FlagsType>(openMode | std::ios::trunc);
	}
	if (lib::HasAnyFlag(flags, EFileOpenFlags::StartAtTheEnd))
	{
		openMode = static_cast<FlagsType>(openMode | std::ios::ate);
	}
	if (lib::HasAnyFlag(flags, EFileOpenFlags::Binary))
	{
		openMode = static_cast<FlagsType>(openMode | std::ios::binary);
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

	return outputStream;
}

Bool File::Exists(const lib::String& path)
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

} // spt::lib
