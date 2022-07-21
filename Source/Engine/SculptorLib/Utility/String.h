#pragma once

#include <string>


namespace spt::lib
{

class String : public std::string
{
protected:

	using Super = std::string;

public:

	// All constructors from base class
	using Super::Super;
};


class StringView : public std::string_view
{
protected:

	using Super = std::string_view;

public:

	// All constructors from base class
	using Super::Super;

	StringView(const String& string)
		: Super(string)
	{ }
};

}