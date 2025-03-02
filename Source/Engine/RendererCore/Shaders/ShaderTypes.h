#pragma once

#include "SculptorCoreTypes.h"
#include "RendererUtils.h"


namespace spt::rdr
{

enum class EShaderFlags : Flags32
{
	None			= 0
};


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

	Bool operator==(ShaderID rhs) const
	{
		return GetID() == rhs.GetID();
	}

	Bool operator!=(ShaderID rhs) const
	{
		return !(*this == rhs);
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


namespace spt::lib
{

template<>
struct Hasher<rdr::ShaderID>
{
	constexpr size_t operator()(const rdr::ShaderID& state) const
	{
		return GetHash(state.GetID());
	}
};

} // spt::lib