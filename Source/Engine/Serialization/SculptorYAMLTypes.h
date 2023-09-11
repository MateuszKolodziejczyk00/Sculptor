#pragma once

#include "ContainersYAMLSerialization.h"
#include "YAMLSerializerHelper.h"

namespace spt::srl
{

template<>
struct TypeSerializer<lib::HashedString>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		if constexpr (Serializer::IsLoading())
		{
			lib::String str;
			serializer.Serialize("Name", str);
			data = str;
		}
		else
		{
			serializer.Serialize("Name", data.ToString());
		}
	}
};

} // spt::srl


SPT_YAML_SERIALIZATION_TEMPLATES(spt::lib::HashedString);
