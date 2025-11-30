#pragma once

#include "SculptorCoreTypes.h"
#include "YAML.h"


namespace YAML
{

// spt::lib::HashMap ==========================================================

template<typename TKey, typename TValue>
inline Emitter& operator<<(Emitter& emitter, const spt::lib::HashMap<TKey, TValue>& hashMap)
{
	emitter << YAML::BeginMap;

	for (const auto& record : hashMap)
	{
		emitter << YAML::Key << record.first << YAML::Value << record.second;
	}

	emitter << YAML::EndMap;

	return emitter;
}

// Convert for unordered_map is already implemented in YAML

} // YAML
