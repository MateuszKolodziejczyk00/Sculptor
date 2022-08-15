#pragma once

#include "SculptorCoreTypes.h"


namespace spt::sc
{

namespace meta
{

static const lib::HashedString buffer		= "Buffer";
static const lib::HashedString exposeInner	= "ExposeInner";

} // meta

struct ShaderParamMetaData
{
public:

	ShaderParamMetaData() = default;

	void AddMeta(lib::HashedString metaParam)
	{
		m_metaParams.emplace(metaParam);
	}

	Bool HasMeta(lib::HashedString metaParam) const
	{
		return m_metaParams.find(metaParam) != std::cend(m_metaParams);
	}

private:

	lib::HashSet<lib::HashedString> m_metaParams;
};


struct ShaderParametersMetaData
{
public:

	ShaderParametersMetaData() =default;

	void AddParamMetaData(lib::HashedString param, const ShaderParamMetaData& metaData)
	{
		m_paramsMetaData.emplace(param, metaData);
	}

	Bool HasMeta(lib::HashedString paramName, lib::HashedString metaParam) const
	{
		const auto foundParamMetaData = m_paramsMetaData.find(paramName);
		return foundParamMetaData != std::cend(m_paramsMetaData) ? foundParamMetaData->second.HasMeta(metaParam) : false;
	}

private:

	lib::HashMap<lib::HashedString, ShaderParamMetaData> m_paramsMetaData;
};

} // spt::sc
