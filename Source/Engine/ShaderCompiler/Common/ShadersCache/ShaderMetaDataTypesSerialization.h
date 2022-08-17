#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaDataTypes.h"
#include "YAML.h"
#include "YAMLSerializerHelper.h"


namespace spt::srl
{

// spt::smd::CommonBindingData ==========================================================

template<>
struct TypeSerializer<smd::CommonBindingData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("ElementsNum", data.m_elementsNum);
		serializer.Serialize("Flags", data.m_flags);
	}
};

// smd::TextureBindingData =========================================================

template<>
struct TypeSerializer<smd::TextureBindingData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		SerializeType<smd::CommonBindingData>(serializer, data);
	}
};

// smd::CombinedTextureSamplerBindingData ==========================================

template<>
struct TypeSerializer<smd::CombinedTextureSamplerBindingData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		SerializeType<smd::CommonBindingData>(serializer, data);
	}
};

// smd::UniformBufferBindingData ===================================================

template<>
struct TypeSerializer<smd::UniformBufferBindingData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		SerializeType<smd::CommonBindingData>(serializer, data);
	}
};

// smd::StorageBufferBindingData ===================================================

template<>
struct TypeSerializer<smd::StorageBufferBindingData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		SerializeType<smd::CommonBindingData>(serializer, data);
		if (!data.IsUnbound())
		{
			if constexpr (Serializer::IsWriting())
			{
				serializer.Serialize("Size", data.GetSize());
			}
			else
			{
				Uint16 size = 0;
				serializer.Serialize("Size", size);
				data.SetSize(size);
			}
		}
	}
};

// smd::ShaderParamEntryCommon =====================================================

template<>
struct TypeSerializer<smd::ShaderParamEntryCommon>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("SetIdx", data.m_setIdx);
		serializer.Serialize("BindingIdx", data.m_bindingIdx);
	}
};

// smd::ShaderTextureParamEntry ====================================================

template<>
struct TypeSerializer<smd::ShaderTextureParamEntry>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		SerializeType<smd::ShaderParamEntryCommon>(serializer, data);
	}
};

// smd::ShaderCombinedTextureSamplerParamEntry =====================================

template<>
struct TypeSerializer<smd::ShaderCombinedTextureSamplerParamEntry>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		SerializeType<smd::ShaderParamEntryCommon>(serializer, data);
	}
};

// smd::ShaderDataParamEntry =======================================================

template<>
struct TypeSerializer<smd::ShaderDataParamEntry>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		SerializeType<smd::ShaderParamEntryCommon>(serializer, data);
		serializer.Serialize("Offset", data.m_offset);
		serializer.Serialize("Size", data.m_size);
		serializer.Serialize("Stride", data.m_stride);
	}
};

// smd::ShaderBufferParamEntry =====================================================

template<>
struct TypeSerializer<smd::ShaderBufferParamEntry>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		SerializeType<smd::ShaderParamEntryCommon>(serializer, data);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::CommonBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::TextureBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::CombinedTextureSamplerBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::UniformBufferBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::StorageBufferBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderTextureParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderCombinedTextureSamplerParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderDataParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderBufferParamEntry)
