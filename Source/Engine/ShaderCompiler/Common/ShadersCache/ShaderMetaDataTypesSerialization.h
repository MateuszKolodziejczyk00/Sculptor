#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaData.h"

#include "YAML.h"
#include "YAMLSerializerHelper.h"


namespace spt::srl
{

template<>
struct TypeSerializer<smd::ShaderMetaData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("DescriptorSetTypeIDs", data.m_dsStateTypeIDs);
		serializer.Serialize("IsBindless", data.m_isBindless);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderMetaData)
