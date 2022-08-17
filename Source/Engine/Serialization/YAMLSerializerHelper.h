#pragma once

#include "SculptorCoreTypes.h"
#include "DataSerializer.h"


namespace spt::srl
{

class YAMLWriter
{
public:

	YAMLWriter(YAML::Node& node)
		: m_node(node)
	{ }

	template<typename TDataType>
	void Serialize(const TDataType& data) const
	{
		m_node.push_back(data);
	}

private:

	YAML::Node& m_node;
};


class YAMLLoader
{
public:

	YAMLLoader(const YAML::Node& node)
		: m_node(node)
		, m_currentIdx(0)
	{ }

	template<typename TDataType>
	void Serialize(TDataType& data)
	{
		data = m_node[m_currentIdx++].as<TDataType>();
	}

private:

	const YAML::Node&	m_node;
	SizeType			m_currentIdx;
};


template<typename TDataType>
class YAMLSerializer
{
public:

	// Writer if type is const, otherwise loader
	using Type = std::conditional_t<std::is_const_v<TDataType>, YAMLWriter, YAMLLoader>;
};

}