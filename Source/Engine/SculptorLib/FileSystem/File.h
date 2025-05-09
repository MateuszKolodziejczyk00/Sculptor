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

	static std::ifstream	OpenInputStream(const lib::String& path, EFileOpenFlags openFlags = EFileOpenFlags::None);
	static std::ofstream	OpenOutputStream(const lib::String& path, EFileOpenFlags openFlags = EFileOpenFlags::None);

	static Bool				Exists(const lib::StringView& path);

	static lib::String		DiscardExtension(const lib::String& file);
	static lib::StringView	GetExtension(const lib::StringView& file);

	struct SCULPTOR_LIB_API Utils
	{
		static lib::String CreateFileNameFromTime(lib::StringView extension = {});
	};
};

} // spt::lib