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
		value = GetImpl<Bool>(name);
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
		value = GetImpl<Int32>(name);
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
		value = GetImpl<Uint32>(name);
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
		value = GetImpl<SizeType>(name);
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
		value = GetImpl<Real32>(name);
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
		value = GetImpl<lib::String>(name);
	}
}

void Serializer::Serialize(const char* name, lib::HashedString& value)
{
	if (IsSaving())
	{
		SetImpl(name, value.ToString());
	}
	else
	{
		value = lib::HashedString(GetImpl<lib::String>(name));
	}
}

void Serializer::Serialize(const char* name, lib::Path& value)
{
	if (IsSaving())
	{
		SetImpl(name, value.generic_string());
	}
	else
	{
		value = lib::Path(GetImpl<lib::String>(name));
	}
}

void Serializer::Serialize(const char* name, lib::RuntimeTypeInfo& value)
{
	if (IsSaving())
	{
		SetImpl(name, lib::String(value.name));
	}
	else
	{
		const lib::HashedString typeName = GetImpl<lib::String>(name);
		value = lib::RuntimeTypeInfo::CreateFromName(typeName.GetView());
	}
}

} // spt::srl
