#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaDataTypes.h"
#include "ShaderMetaData.h"

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
		serializer.Serialize("ElementsNum", data.elementsNum);
		serializer.SerializeEnum("Flags", data.flags);
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

// smd::GenericShaderBinding =======================================================

template<>
struct TypeSerializer<smd::GenericShaderBinding>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		if constexpr (Serializer::IsWriting())
		{
			if (data.Contains<smd::TextureBindingData>())
			{
				serializer.Serialize("TypeIdx", 0);
				serializer.Serialize("Data", data.As<smd::TextureBindingData>());
			}
			else if (data.Contains<smd::CombinedTextureSamplerBindingData>())
			{
				serializer.Serialize("TypeIdx", 1);
				serializer.Serialize("Data", data.As<smd::CombinedTextureSamplerBindingData>());
			}
			else if (data.Contains<smd::UniformBufferBindingData>())
			{
				serializer.Serialize("TypeIdx", 2);
				serializer.Serialize("Data", data.As<smd::UniformBufferBindingData>());
			}
			else if (data.Contains<smd::StorageBufferBindingData>())
			{
				serializer.Serialize("TypeIdx", 3);
				serializer.Serialize("Data", data.As<smd::StorageBufferBindingData>());
			}
			else
			{
				SPT_CHECK_NO_ENTRY(); // Invalid type of binding
			}
		}
		else
		{
			Int32 typeIdx = idxNone<Int32>;
			serializer.Serialize("TypeIdx", typeIdx);
			SPT_CHECK(typeIdx != idxNone<Int32>);

			if (typeIdx == 0)
			{
				smd::TextureBindingData bindingData;
				serializer.Serialize("Data", bindingData);
				data.Set(bindingData);
			}
			else if (typeIdx == 1)
			{
				smd::CombinedTextureSamplerBindingData bindingData;
				serializer.Serialize("Data", bindingData);
				data.Set(bindingData);
			}
			else if (typeIdx == 2)
			{
				smd::UniformBufferBindingData bindingData;
				serializer.Serialize("Data", bindingData);
				data.Set(bindingData);
			}
			else if (typeIdx == 3)
			{
				smd::StorageBufferBindingData bindingData;
				serializer.Serialize("Data", bindingData);
				data.Set(bindingData);
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
		serializer.Serialize("SetIdx", data.setIdx);
		serializer.Serialize("BindingIdx", data.bindingIdx);
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
		serializer.Serialize("Offset", data.offset);
		serializer.Serialize("Size", data.size);
		serializer.Serialize("Stride", data.stride);
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

// smd::GenericShaderParamEntry ====================================================

template<>
struct TypeSerializer<smd::GenericShaderParamEntry>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		if constexpr (Serializer::IsWriting())
		{
			if (data.Contains<smd::ShaderTextureParamEntry>())
			{
				serializer.Serialize("TypeIdx", 0);
				serializer.Serialize("Data", data.As<smd::ShaderTextureParamEntry>());
			}
			else if (data.Contains<smd::ShaderCombinedTextureSamplerParamEntry>())
			{
				serializer.Serialize("TypeIdx", 1);
				serializer.Serialize("Data", data.As<smd::ShaderCombinedTextureSamplerParamEntry>());
			}
			else if (data.Contains<smd::ShaderDataParamEntry>())
			{
				serializer.Serialize("TypeIdx", 2);
				serializer.Serialize("Data", data.As<smd::ShaderDataParamEntry>());
			}
			else if (data.Contains<smd::ShaderBufferParamEntry>())
			{
				serializer.Serialize("TypeIdx", 3);
				serializer.Serialize("Data", data.As<smd::ShaderBufferParamEntry>());
			}
			else
			{
				SPT_CHECK_NO_ENTRY(); // Invalid type of binding
			}
		}
		else
		{
			Int32 typeIdx = idxNone<Int32>;
			serializer.Serialize("TypeIdx", typeIdx);
			SPT_CHECK(typeIdx != idxNone<Int32>);

			if (typeIdx == 0)
			{
				smd::ShaderTextureParamEntry paramEntry;
				serializer.Serialize("Data", paramEntry);
				data.Set(paramEntry);
			}
			else if (typeIdx == 1)
			{
				smd::ShaderCombinedTextureSamplerParamEntry paramEntry;
				serializer.Serialize("Data", paramEntry);
				data.Set(paramEntry);
			}
			else if (typeIdx == 2)
			{
				smd::ShaderDataParamEntry paramEntry;
				serializer.Serialize("Data", paramEntry);
				data.Set(paramEntry);
			}
			else if (typeIdx == 3)
			{
				smd::ShaderBufferParamEntry paramEntry;
				serializer.Serialize("Data", paramEntry);
				data.Set(paramEntry);
			}
		}
	}
};

// smd::ShaderDescriptorSet ========================================================

template<>
struct TypeSerializer<smd::ShaderDescriptorSet>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("Bindings", data.GetBindings());
	}
};

// smd::ShaderMetaData =============================================================

template<>
struct TypeSerializer<smd::ShaderMetaData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("DescriptorSets", data.m_descriptorSets);
		serializer.Serialize("ParameterMap", data.m_parameterMap);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::CommonBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::TextureBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::CombinedTextureSamplerBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::UniformBufferBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::StorageBufferBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::GenericShaderBinding)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderTextureParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderCombinedTextureSamplerParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderDataParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderBufferParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::GenericShaderParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderDescriptorSet)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderMetaData)
