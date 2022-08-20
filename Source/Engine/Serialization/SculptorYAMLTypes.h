#pragma once

#include "SculptorCoreTypes.h"
#include "YAML.h"
#include "ContainersYAMLSerialization.h"

namespace YAML
{

// spt::lib::HashedString =====================================================

inline Emitter& operator<<(Emitter& emitter, spt::lib::HashedString hashedString)
{
	return emitter.Write(hashedString.ToString());
}

template<>
struct convert<spt::lib::HashedString>
{
	static Node encode(const spt::lib::HashedString& hashedString)
	{
		const spt::lib::String asString = hashedString.ToString();

		Node node;
		node.push_back(asString);
		return node;
	}

	static bool decode(const Node& node, spt::lib::HashedString& hashedString)
	{
		const spt::lib::String asString = node[0].as<spt::lib::String>();
		hashedString = asString;
		return true;
	}
};

} // YAML
