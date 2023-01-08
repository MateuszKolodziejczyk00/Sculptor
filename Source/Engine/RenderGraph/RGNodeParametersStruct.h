#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResources.h"


namespace spt::rg
{

namespace priv
{

using InvalidAccessType = SizeType;

template<typename TParameterType>
struct ParameterTypeUtils
{
	using AccessType = InvalidAccessType;
};


template<>
struct ParameterTypeUtils<RGBufferViewHandle>
{
	using AccessType = ERGBufferAccess;
};


template<>
struct ParameterTypeUtils<RGTextureViewHandle>
{
	using AccessType = ERGTextureAccess;
};


template<typename TStruct, typename TNextParameterAccessType, typename TParameterType, typename ParameterTypeUtils<TParameterType>::AccessType access, rhi::EShaderStageFlags shaderStages, lib::Literal name>
class RGParameterAccessMetaData
{
public:

	using AccessType		= typename ParameterTypeUtils<TParameterType>::AccessType;
	using ParameterType		= TParameterType;
	using NextParamMetaData	= TNextParameterAccessType;

	SPT_STATIC_CHECK_MSG(!std::is_same_v<AccessType, InvalidAccessType>, "Invalid access type for parameter");
	
	RGParameterAccessMetaData() = default;

	static constexpr SizeType GetOffset()
	{
		// this is first parameter in the struct
		if constexpr (std::is_same_v<NextParamMetaData, void>)
		{
			return 0;
		}

		using PrevMemberType = typename NextParamMetaData::ParameterType;

		constexpr SizeType prevMemberEndOffset = NextParamMetaData::GetOffset() + sizeof(PrevMemberType);
		constexpr SizeType thisMemberAlignment = alignof(ParameterType);
		constexpr SizeType offset = math::Utils::RoundUp(prevMemberEndOffset, thisMemberAlignment);
		return offset;
	}

	static constexpr const char* GetName()
	{
		return name.Get();
	}

	static constexpr AccessType GetAccess()
	{
		return access;
	}

	static constexpr rhi::EShaderStageFlags GetShaderStages()
	{
		return shaderStages;
	}

	static TParameterType& GetParameter(TStruct& parametersStruct)
	{
		Byte* paramtersStructPtr = reinterpret_cast<Byte*>(&parametersStruct);
		Byte* paramterPtr = paramtersStructPtr + GetOffset();
		return *reinterpret_cast<TParameterType*>(paramterPtr);
	}

	static const TParameterType& GetParameter(const TStruct& parametersStruct)
	{
		const Byte* paramtersStructPtr = reinterpret_cast<const Byte*>(&parametersStruct);
		const Byte* paramterPtr = paramtersStructPtr + GetOffset();
		return *reinterpret_cast<const TParameterType*>(paramterPtr);
	}
};


template<typename TStruct, typename TNextParameterAccessType>
class RGParameterAccessMetaData<TStruct, TNextParameterAccessType, void, 0, rhi::EShaderStageFlags::None, "Head">
{
public:

	using AcccessType		= void;
	using ParameterType		= void;
	using NextParamMetaData	= TNextParameterAccessType;
	
	RGParameterAccessMetaData() = default;
};

} // priv

#define BEGIN_RG_NODE_PARAMETERS_STRUCT(name) \
struct name \
{ \
private: \
	using ThisClass = name; \
public: \
	static constexpr const char* GetStructName() \
	{ \
		return #name; \
	} \
	typedef spt::rg::priv::RGParameterAccessMetaData<ThisClass, void

#define PARAMETER(type, name, access, shaderStages) \
	, type, access, shaderStages, #name> _MT_##name; \
	type name; \
	typedef spt::rg::priv::RGParameterAccessMetaData<ThisClass, _MT_##name

#define END_RG_NODE_PARAMETERS_STRUCT() \
	, void, 0, rhi::EShaderStageFlags::None, "Head"> MetaDataHead; \
};

#define RG_BUFFER_VIEW(name, access, shaderStages) PARAMETER(spt::rg::RGBufferViewHandle, name, access, shaderStages)
#define RG_TEXTURE_VIEW(name, access, shaderStages) PARAMETER(spt::rg::RGTextureViewHandle, name, access, shaderStages)


namespace priv
{
template<typename TCurrentParameterMetaData, typename TRGParametersStruct, typename TCallable>
void ForEachRGParameterAccess(const TRGParametersStruct& parameters, TCallable&& callable)
{
	// If current parameter is valid
	if constexpr (!std::is_same_v<typename TCurrentParameterMetaData::ParameterType, void>)
	{
		callable(TCurrentParameterMetaData::GetParameter(parameters), TCurrentParameterMetaData::GetAccess(), TCurrentParameterMetaData::GetShaderStages());
	}

	// If has next parameter
	if constexpr (!std::is_same_v<typename TCurrentParameterMetaData::NextParamMetaData, void>)
	{
		ForEachRGParameterAccess<typename TCurrentParameterMetaData::NextParamMetaData>(parameters, callable);
	}
}

} // priv


template<typename TRGParametersStruct, typename TCallable>
void ForEachRGParameterAccess(const TRGParametersStruct& parameters, TCallable&& callable)
{
	priv::ForEachRGParameterAccess<typename TRGParametersStruct::MetaDataHead>(parameters, callable);
}

} // spt::rg