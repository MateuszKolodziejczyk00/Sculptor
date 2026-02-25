#pragma once

#include "ShaderStructsMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rdr
{

#if SHADER_STRUCTS_RTTI

struct ShaderStructMemberRTTI
{
	lib::String memberName;
	lib::String memberTypeName;
	Uint32      offset = 0;
	Uint32      elementsNum = 1;
	Uint32      stride = 0;
};

struct ShaderStructRTTI
{
	lib::String structName;
	Uint32      size = 0u;
	lib::DynamicArray<ShaderStructMemberRTTI> members;
};
//struct 

#endif // SHADER_STRUCTS_RTTI

class SHADER_STRUCTS_API ShaderStructMetaData
{
public:

	ShaderStructMetaData();

	explicit ShaderStructMetaData(lib::String hlslSourceCode, Uint64 versionHash);

#if SHADER_STRUCTS_RTTI
	void SetRTTI(const ShaderStructRTTI& rtti) { m_rtti = std::move(rtti); }
	const ShaderStructRTTI& GetRTTI() const { return m_rtti; }
#endif // SHADER_STRUCTS_RTTI

	const lib::String& GetHLSLSourceCode() const { return m_hlslSourceCode; }

	Uint64 GetVersionHash() const { return m_versionHash; }

private:

	lib::String m_hlslSourceCode;

	Uint64 m_versionHash = 0u;

#if SHADER_STRUCTS_RTTI
	ShaderStructRTTI m_rtti;
#endif // SHADER_STRUCTS_RTTI
};



struct StructsRegistryData {};


class SHADER_STRUCTS_API ShaderStructsRegistry
{
public:

	static StructsRegistryData* GetRegistryData();
	static void InitializeModule(StructsRegistryData* data);

	static void RegisterStructMetaData(lib::String structName, ShaderStructMetaData structMetaData);
	static const ShaderStructMetaData& GetStructMetaDataChecked(const lib::String& structName);
	static const ShaderStructMetaData* GetStructMetaData(const lib::String& structName);

private:

	ShaderStructsRegistry() = default;
};

} // spt::rdr
