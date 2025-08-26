#pragma once

#include "ShaderStructsMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rdr
{

#if SHADER_STRUCTS_RTTI

struct ShaderStructMemberRTTI
{
	lib::HashedString memberName;
	lib::HashedString memberTypeName;
	Uint32            offset = 0;
	Uint32            elementsNum = 1;
	Uint32            stride = 0;
};

struct ShaderStructRTTI
{
	lib::HashedString structName;
	Uint32            size = 0u;
	lib::DynamicArray<ShaderStructMemberRTTI> members;
};
//struct 

#endif // SHADER_STRUCTS_RTTI

class SHADER_STRUCTS_API ShaderStructMetaData
{
public:

	ShaderStructMetaData();

	explicit ShaderStructMetaData(lib::String hlslSourceCode);

#if SHADER_STRUCTS_RTTI
	void SetRTTI(const ShaderStructRTTI& rtti) { m_rtti = std::move(rtti); }
	const ShaderStructRTTI& GetRTTI() const { return m_rtti; }
#endif // SHADER_STRUCTS_RTTI

	const lib::String& GetHLSLSourceCode() const { return m_hlslSourceCode; }

private:

	lib::String m_hlslSourceCode;

#if SHADER_STRUCTS_RTTI
	ShaderStructRTTI m_rtti;
#endif // SHADER_STRUCTS_RTTI
};


class SHADER_STRUCTS_API ShaderStructsRegistry
{
public:

	static void RegisterStructMetaData(lib::HashedString structName, ShaderStructMetaData structMetaData);
	static const ShaderStructMetaData& GetStructMetaDataChecked(lib::HashedString structName);
	static const ShaderStructMetaData* GetStructMetaData(lib::HashedString structName);

private:

	ShaderStructsRegistry() = default;
};

} // spt::rdr