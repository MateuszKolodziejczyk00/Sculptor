#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"
#include "DependenciesBuilder.h"


namespace spt::gfx
{

class NamedBufferBase
{
public:

	NamedBufferBase() = default;

	NamedBufferBase& operator=(const rdr::BindableBufferView& bufferView)
	{
		m_boundBufferView = &bufferView;
		return *this;
	}

	Uint32 GetDescriptorIdx() const
	{
		return m_boundBufferView ? m_boundBufferView->GetUAVDescriptor().Get() : rdr::invalidResourceDescriptorIdx;
	}

private:

	const rdr::BindableBufferView* m_boundBufferView = nullptr;
};


template<lib::Literal name>
class NamedBuffer : public NamedBufferBase
{
public:

	NamedBuffer() = default;

	using NamedBufferBase::operator=;

	static constexpr const char* GetName()
	{
		return name.Get();
	}

};


#define CREATE_NAMED_BUFFER(name) \
using name = gfx::NamedBuffer<lib::Literal(#name)>

template<typename TType>
concept CNamedBuffer = std::is_base_of_v<NamedBufferBase, TType>;

} // spt::gfx

namespace spt::rdr::shader_translator
{

template<lib::Literal name>
struct StructTranslator<gfx::NamedBuffer<name>>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return "NamedBufferDescriptorIdx";
	}
};


template<lib::Literal name>
struct StructCPPToHLSLTranslator<gfx::NamedBuffer<name>>
{
	static void Copy(const gfx::NamedBuffer<name>& cppData, lib::Span<Byte> hlslData)
	{
		SPT_CHECK(hlslData.size() == 4u);
		Uint32* hlsl = reinterpret_cast<Uint32*>(hlslData.data());
		hlsl[0] = cppData.GetDescriptorIdx();
	}
};


template<lib::Literal name>
struct StructHLSLSizeEvaluator<gfx::NamedBuffer<name>>
{
	static constexpr Uint32 Size()
	{
		return 4u;
	}
};


template<lib::Literal name>
struct StructHLSLAlignmentEvaluator<gfx::NamedBuffer<name>>
{
	static constexpr Uint32 Alignment()
	{
		return 4u;
	}
};


constexpr lib::String DeclareNamedBufferHLSLAccessor(const char* namedBufferName)
{
	lib::String code = lib::String("uint ___Get") + namedBufferName + "();\n";
	code += "struct Accessor_" + lib::String(namedBufferName) + " {\nstatic uint Get() { return ___Get" + namedBufferName + "(); }\n};\n";
	return code;
}


template<typename TShaderStructMemberMetaData>
constexpr void DeclareNamedBufferHLSLAccessors(lib::String& code)
{
	if constexpr (!priv::IsTailMember<TShaderStructMemberMetaData>())
	{
		DeclareNamedBufferHLSLAccessors<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(code);
	}
	
	if constexpr (!priv::IsHeadMember<TShaderStructMemberMetaData>())
	{
		using MemberElementType = typename TShaderStructMemberMetaData::ElementType;
		if constexpr (gfx::CNamedBuffer<MemberElementType>)
		{
			code += DeclareNamedBufferHLSLAccessor(MemberElementType::GetName());
		}
		else if constexpr (IsShaderStruct<MemberElementType>())
		{
			DeclareNamedBufferHLSLAccessors<typename MemberElementType::HeadMemberMetaData>(code);
		}
	}
}


template<typename TStruct>
constexpr lib::String DeclareNamedBufferHLSLAccessors()
{
	lib::String code;
	DeclareNamedBufferHLSLAccessors<typename TStruct::HeadMemberMetaData>(INOUT code);
	return code;
}


constexpr lib::String DefineNamedBufferHLSLAccessor(const char* namedBufferName, const lib::String& accessor)
{
	return lib::String("uint ___Get") + namedBufferName + "() { return " + accessor + ".idx; }\n";
}


template<typename TShaderStructMemberMetaData>
constexpr void DefineNamedBufferHLSLAccessors(lib::String& code, const lib::String& currentAccessor)
{
	if constexpr (!priv::IsTailMember<TShaderStructMemberMetaData>())
	{
		DefineNamedBufferHLSLAccessors<typename TShaderStructMemberMetaData::PrevMemberMetaDataType>(code, currentAccessor);
	}
	
	if constexpr (!priv::IsHeadMember<TShaderStructMemberMetaData>())
	{
		using MemberElementType = typename TShaderStructMemberMetaData::ElementType;
		if constexpr (gfx::CNamedBuffer<MemberElementType>)
		{
			const lib::String accessor = currentAccessor + TShaderStructMemberMetaData::GetVariableName();
			code += DefineNamedBufferHLSLAccessor(MemberElementType::GetName(), accessor);
		}
		else if constexpr (IsShaderStruct<MemberElementType>())
		{
			const lib::String newAccessor = currentAccessor + TShaderStructMemberMetaData::GetVariableName() + ".";
			DefineNamedBufferHLSLAccessors<typename MemberElementType::HeadMemberMetaData>(code, newAccessor);
		}
	}
}


template<typename TStruct>
constexpr lib::String DefineNamedBufferHLSLAccessors(const lib::String& currentAccessor)
{
	lib::String code;
	DefineNamedBufferHLSLAccessors<typename TStruct::HeadMemberMetaData>(INOUT code, currentAccessor);
	return code;
}

} // spt::rdr::shader_translator

namespace spt::rg
{

template<lib::Literal name>
struct HLSLStructDependenciesBuider<gfx::NamedBuffer<name>>
{
	static void CollectDependencies(lib::Span<const Byte> hlsl, RGDependenciesBuilder& dependenciesBuilder)
	{
		SPT_CHECK(hlsl.size() == 4u);

		const Uint32* hlslData = reinterpret_cast<const Uint32*>(hlsl.data());

		const rdr::ResourceDescriptorIdx descriptoridx = rdr::ResourceDescriptorIdx(hlslData[0]);

		if (descriptoridx != rdr::invalidResourceDescriptorIdx)
		{
			rg::RGBufferAccessInfo accessInfo;
			accessInfo.access = rg::ERGBufferAccess::Read;
			dependenciesBuilder.AddBufferAccess(descriptoridx, accessInfo);
		}
	}
};

} // spt::rg