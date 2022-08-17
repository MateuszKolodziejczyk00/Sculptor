#pragma once

#include "SculptorCoreTypes.h"
#include "YAML.h"

#include <variant>

namespace spt::srl
{

class DataSerializer
{
public:

	DataSerializer();

	void		StartWriting(YAML::Node& node);
	void		StartLoading(const YAML::Node& node);

	Bool		IsValid() const;

	Bool		IsWriting() const;
	Bool		IsLoading() const;

	template<typename TData>
	void		Serialize(TData& data);

	template<typename TData>
	void		Write(const TData& data) const;

	template<typename TData>
	void		Load(TData& data);

private:

	union
	{
		YAML::Node*		m_writtedNode;
		SizeType		m_currentIdx;
	};

	const YAML::Node*	m_loadedNode;
	
	Bool				m_isWriting;
};


inline DataSerializer::DataSerializer()
	: m_writtedNode(nullptr)
	, m_loadedNode(nullptr)
	, m_isWriting(false)
{ }

inline void DataSerializer::StartWriting(YAML::Node& node)
{
	m_loadedNode = nullptr;
	m_writtedNode = &node;
	m_isWriting = true;
}

inline void DataSerializer::StartLoading(const YAML::Node& node)
{
	m_loadedNode = &node;
	m_currentIdx = 0;
	m_isWriting = false;
}

inline Bool DataSerializer::IsValid() const
{
	return !!m_loadedNode || !!m_writtedNode;
}

inline Bool DataSerializer::IsWriting() const
{
	SPT_CHECK(IsValid());
	return m_isWriting;
}

inline Bool DataSerializer::IsLoading() const
{
	return !IsWriting();
}

template<typename TData>
void DataSerializer::Serialize(TData& data)
{
	if(IsWriting())
	{
		Write(data);
	}
	else
	{
		Load(data);
	}
}

template<typename TData>
void DataSerializer::Write(const TData& data) const
{
	m_writtedNode->push_back(data);
}

template<typename TData>
void DataSerializer::Load(TData& data)
{
	data = m_loadedNode[m_currentIdx++].as<TData>();
}

} // spt::srl