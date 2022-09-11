#pragma once

#include "SculptorCoreTypes.h"

#include <fstream>

namespace spt::lib
{

enum class EFileOpenFlags
{
	None			= 0,
	ForceCreate		= BIT(0), //  create directory if needed etc.
	DiscardContent	= BIT(1),
	StartAtTheEnd	= BIT(2),
	Binary			= BIT(3)
};


class SCULPTORLIB_API File
{
public:

	static std::ifstream	OpenInputStream(const lib::String& path, EFileOpenFlags openFlags = EFileOpenFlags::None);
	static std::ofstream	OpenOutputStream(const lib::String& path, EFileOpenFlags openFlags = EFileOpenFlags::None);

	static Bool				Exists(const lib::String& path);

	static lib::String		DiscardExtension(const lib::String& file);
	static lib::String		GetExtension(const lib::String& file);
};

} // spt::lib