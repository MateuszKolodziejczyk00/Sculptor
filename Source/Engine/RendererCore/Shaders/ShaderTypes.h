#pragma once

#include "SculptorCoreTypes.h"
#include "RendererUtils.h"


namespace spt::rdr
{

using ShaderHashType = SizeType;


class ShaderID
{
public:

	ShaderID()
		: m_id(idxNone<ShaderHashType>)
	{ }

	ShaderID(ShaderHashType inID, RendererResourceName inName)
		: m_id(inID)
		, m_name(inName)
	{ }

	Bool IsValid() const
	{
		return m_id != idxNone<ShaderHashType>;
	}

	ShaderHashType GetID() const
	{
		return m_id;
	}

	const lib::HashedString& GetName() const
	{
		return m_name.Get();
	}

private:

	ShaderHashType			m_id;
	RendererResourceName	m_name;
};

} // spt::rdr


namespace std
{

template<>
struct hash<spt::rdr::ShaderID>
{
	size_t operator()(const spt::rdr::ShaderID& state) const
	{
		return spt::lib::GetHash(state.GetID());
	}
};

} // std