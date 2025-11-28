#pragma once

#include "SculptorCoreTypes.h"
#include <filesystem>

#include <fstream>

namespace spt::lib
{

using Path = std::filesystem::path;


enum class EFileOpenFlags
{
	None			= 0,
	ForceCreate		= BIT(0), //  create directory if needed etc.
	DiscardContent	= BIT(1),
	StartAtTheEnd	= BIT(2),
	Binary			= BIT(3)
};


class SCULPTOR_LIB_API File
{
public:

	static std::ifstream OpenInputStream(const String& path, EFileOpenFlags openFlags = EFileOpenFlags::None);
	static std::ifstream OpenInputStream(const Path& path, EFileOpenFlags openFlags = EFileOpenFlags::None);

	static std::ofstream OpenOutputStream(const String& path, EFileOpenFlags openFlags = EFileOpenFlags::None);
	static std::ofstream OpenOutputStream(const Path& path, EFileOpenFlags openFlags = EFileOpenFlags::None);

	static String ReadDocument(const Path& path, EFileOpenFlags openFlags = EFileOpenFlags::None);

	static Bool Exists(const StringView& path);

	static String     DiscardExtension(const String& file);
	static StringView GetExtension(const StringView& file);

	struct SCULPTOR_LIB_API Utils
	{
		static String CreateFileNameFromTime(StringView extension = {});
	};
};

} // spt::lib