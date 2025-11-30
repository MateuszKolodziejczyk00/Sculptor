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

	if (HasAnyFlag(flags, EFileOpenFlags::DiscardContent))
	{
		AddFlag(openMode, std::ios::trunc);
	}
	if (HasAnyFlag(flags, EFileOpenFlags::StartAtTheEnd))
	{
		AddFlag(openMode, std::ios::ate);
	}
	if (HasAnyFlag(flags, EFileOpenFlags::Binary))
	{
		AddFlag(openMode, std::ios::binary);
	}

	return openMode;
}

} // priv

std::ifstream File::OpenInputStream(const String& path, EFileOpenFlags openFlags /*= EFileOpenFlags::None*/)
{
	return OpenInputStream(Path(path), openFlags);
}

std::ifstream File::OpenInputStream(const Path& path, EFileOpenFlags openFlags /*= EFileOpenFlags::None*/)
{
	std::ifstream inputStream(path, priv::GetOpenMode(openFlags));
	return inputStream;
}

std::ofstream File::OpenOutputStream(const String& path, EFileOpenFlags openFlags /*= EFileOpenFlags::None*/)
{
	return OpenOutputStream(Path(path), openFlags);
}

std::ofstream File::OpenOutputStream(const Path& path, EFileOpenFlags openFlags /*= EFileOpenFlags::None*/)
{
	std::ofstream outputStream(path, priv::GetOpenMode(openFlags));
	
	if (!outputStream.is_open() && HasAnyFlag(openFlags, EFileOpenFlags::ForceCreate))
	{
		std::filesystem::create_directories(path.parent_path());
		outputStream.open(path);
	}

	SPT_CHECK(!HasAnyFlag(openFlags, EFileOpenFlags::ForceCreate) || !outputStream.bad());

	return outputStream;
}

String File::ReadDocument(const Path& path, EFileOpenFlags openFlags /*= EFileOpenFlags::None*/)
{
	std::ifstream stream = lib::File::OpenInputStream(path);

	lib::String content;

	if (stream.is_open())
	{
		content.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		stream.close();
	}

	return content;
}

void File::SaveDocument(const Path& path, const lib::String& data, EFileOpenFlags openFlags /*= lib::Flags(lib::EFileOpenFlags::ForceCreate, lib::EFileOpenFlags::DiscardContent)*/)
{
	std::ofstream stream = lib::File::OpenOutputStream(path, openFlags);
	SPT_CHECK(stream.is_open());

	stream << data;

	stream.close();
}

Bool File::Exists(const StringView& path)
{
	return std::filesystem::exists(path);
}

String File::DiscardExtension(const String& file)
{
	const SizeType extensionBeginPosition = file.find_last_of('.');
	return extensionBeginPosition != String::npos
		? String(std::cbegin(file), std::cbegin(file) + extensionBeginPosition)
		: file;
}

StringView File::GetExtension(const StringView& file)
{
	const SizeType extensionBeginPosition = file.find_last_of('.');
	return extensionBeginPosition != String::npos
		? StringView(std::cbegin(file) + extensionBeginPosition + 1, std::cend(file))
		: StringView();
}

String File::Utils::CreateFileNameFromTime(StringView extension /*= {}*/)
{
	const auto now       = std::chrono::system_clock::now();
	const auto nowTime   = std::chrono::system_clock::to_time_t(now);

	struct tm buf;
	localtime_s(&buf, &nowTime);

	char buffer[256];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &buf);

	String result = buffer;

	if (!extension.empty())
	{
		result += '.';
		result += extension;
	}

	return result;
}

} // spt::lib
