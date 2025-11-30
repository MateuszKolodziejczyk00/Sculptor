#include "Serialization.h"

namespace spt::srl
{

void Serializer::Serialize(const char* name, Bool& value)
{
	if (IsSaving())
	{
		m_json[name] = value;
	}
	else
	{
		value = m_json[name].get<Bool>();
	}
}

void Serializer::Serialize(const char* name, Int32& value)
{
	if (IsSaving())
	{
		m_json[name] = value;
	}
	else
	{
		value = m_json[name].get<Int32>();
	}
}

void Serializer::Serialize(const char* name, Uint32& value)
{
	if (IsSaving())
	{
		m_json[name] = value;
	}
	else
	{
		value = m_json[name].get<Uint32>();
	}
}

void Serializer::Serialize(const char* name, Real32& value)
{
	if (IsSaving())
	{
		m_json[name] = value;
	}
	else
	{
		value = m_json[name].get<Real32>();
	}
}

void Serializer::Serialize(const char* name, lib::String& value)
{
	if (IsSaving())
	{
		m_json[name] = value;
	}
	else
	{
		value = m_json[name].get<lib::String>();
	}
}

void Serializer::Serialize(const char* name, lib::HashedString& value)
{
	if (IsSaving())
	{
		m_json[name] = value.ToString();
	}
	else
	{
		value = lib::HashedString(m_json[name].get<lib::String>());
	}
}

void Serializer::Serialize(const char* name, lib::Path& value)
{
	if (IsSaving())
	{
		m_json[name] = value.generic_string();
	}
	else
	{
		value = lib::Path(m_json[name].get<lib::String>());
	}
}

} // spt::srl
