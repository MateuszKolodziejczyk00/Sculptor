#include "Serialization.h"


namespace spt::srl
{

void Serializer::Serialize(const char* name, Bool& value)
{
	if (IsSaving())
	{
		SetImpl(name, value);
	}
	else
	{
		GetImpl(name, value);
	}
}

void Serializer::Serialize(const char* name, Int32& value)
{
	if (IsSaving())
	{
		SetImpl(name, value);
	}
	else
	{
		GetImpl(name, value);
	}
}

void Serializer::Serialize(const char* name, Uint32& value)
{
	if (IsSaving())
	{
		SetImpl(name, value);
	}
	else
	{
		GetImpl(name, value);
	}
}

void Serializer::Serialize(const char* name, SizeType& value)
{
	if (IsSaving())
	{
		SetImpl(name, value);
	}
	else
	{
		GetImpl(name, value);
	}
}

void Serializer::Serialize(const char* name, Real32& value)
{
	if (IsSaving())
	{
		SetImpl(name, value);
	}
	else
	{
		GetImpl(name, value);
	}
}

void Serializer::Serialize(const char* name, lib::String& value)
{
	if (IsSaving())
	{
		SetImpl(name, value);
	}
	else
	{
		GetImpl(name, value);
	}
}

void Serializer::Serialize(const char* name, lib::HashedString& value)
{
	if (IsSaving())
	{
		SetImpl(name, value.ToString());
	}
	else if (const JSON* json = FindJson(name))
	{
		const lib::String& str = json->get_ref<const lib::String&>();
		value = str.empty() ? lib::HashedString() : lib::HashedString(str);
	}
}

void Serializer::Serialize(const char* name, lib::Path& value)
{
	if (IsSaving())
	{
		SetImpl(name, value.generic_string());
	}
	else if (const JSON* json = FindJson(name))
	{
		value = lib::Path(json->get_ref<const lib::String&>());
	}
}

void Serializer::Serialize(const char* name, lib::RuntimeTypeInfo& value)
{
	if (IsSaving())
	{
		SetImpl(name, lib::String(value.name));
	}
	else if (const JSON* json = FindJson(name))
	{
		const lib::HashedString typeName = json->get_ref<const lib::String&>();
		value = lib::RuntimeTypeInfo::CreateFromName(typeName.GetView());
	}
}

} // spt::srl
