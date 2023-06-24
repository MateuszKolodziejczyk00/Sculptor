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
		serializer.SerializeEnum("BindingDescriptorType", data.bindingDescriptorType);
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

// smd::BufferBindingData ==========================================================

template<>
struct TypeSerializer<smd::BufferBindingData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		SerializeType<smd::CommonBindingData>(serializer, data);
		if (!data.IsUnbound())
		{
			if constexpr (Serializer::IsSaving())
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

// smd::SamplerBindingData =========================================================

template<>
struct TypeSerializer<smd::SamplerBindingData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		SerializeType<smd::CommonBindingData>(serializer, data);
		
		if constexpr (Serializer::IsSaving())
		{
			const srl::Binary immutableSamplerDef(reinterpret_cast<const unsigned char*>(&data.GetImmutableSamplerDefinition()), sizeof(rhi::SamplerDefinition));
			serializer.Serialize("ImmutableSamplerDef", immutableSamplerDef);
		}
		else
		{
			srl::Binary immutableSamplerDef;
			serializer.Serialize("ImmutableSamplerDef", immutableSamplerDef);
			SPT_CHECK(immutableSamplerDef.size() == sizeof(rhi::SamplerDefinition));
			data.SetImmutableSamplerDefinition(*reinterpret_cast<const rhi::SamplerDefinition*>(immutableSamplerDef.data()));
		}
	}
};

// smd::AccelerationStructureBindingData ===========================================

template<>
struct TypeSerializer<smd::AccelerationStructureBindingData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		SerializeType<smd::CommonBindingData>(serializer, data);
	}
};

// smd::GenericShaderBinding =======================================================

template<>
struct TypeSerializer<smd::GenericShaderBinding>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		if constexpr (Serializer::IsSaving())
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
			else if (data.Contains<smd::BufferBindingData>())
			{
				serializer.Serialize("TypeIdx", 2);
				serializer.Serialize("Data", data.As<smd::BufferBindingData>());
			}
			else if (data.Contains<smd::SamplerBindingData>())
			{
				serializer.Serialize("TypeIdx", 3);
				serializer.Serialize("Data", data.As<smd::SamplerBindingData>());
			}
			else if (data.Contains<smd::AccelerationStructureBindingData>())
			{
				serializer.Serialize("TypeIdx", 4);
				serializer.Serialize("Data", data.As<smd::AccelerationStructureBindingData>());
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
				smd::BufferBindingData bindingData;
				serializer.Serialize("Data", bindingData);
				data.Set(bindingData);
			}
			else if (typeIdx == 3)
			{
				smd::SamplerBindingData bindingData;
				serializer.Serialize("Data", bindingData);
				data.Set(bindingData);
			}
			else if (typeIdx == 4)
			{
				smd::AccelerationStructureBindingData bindingData;
				serializer.Serialize("Data", bindingData);
				data.Set(bindingData);
			}
			else
			{
				SPT_CHECK_NO_ENTRY(); // Invalid type of binding
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

// smd::ShaderAccelerationStructureParamEntry ======================================

template<>
struct TypeSerializer<smd::ShaderAccelerationStructureParamEntry>
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
		if constexpr (Serializer::IsSaving())
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
			else if (data.Contains<smd::ShaderBufferParamEntry>())
			{
				serializer.Serialize("TypeIdx", 2);
				serializer.Serialize("Data", data.As<smd::ShaderBufferParamEntry>());
			}
			else if (data.Contains<smd::ShaderAccelerationStructureParamEntry>())
			{
				serializer.Serialize("TypeIdx", 3);
				serializer.Serialize("Data", data.As<smd::ShaderAccelerationStructureParamEntry>());
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
				smd::ShaderBufferParamEntry paramEntry;
				serializer.Serialize("Data", paramEntry);
				data.Set(paramEntry);
			}
			else if (typeIdx == 3)
			{
				smd::ShaderAccelerationStructureParamEntry paramEntry;
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
		serializer.Serialize("DescriptorSetParametersHashes", data.m_descriptorSetParametersHashes);
		serializer.Serialize("DescriptorSetTypesHashes", data.m_dsStateTypeHashes);
	}
};

// smd::ShaderDataParam ============================================================

template<>
struct TypeSerializer<smd::ShaderDataParam>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("Offset", data.offset);
		serializer.Serialize("Size", data.size);
		serializer.Serialize("Stride", data.stride);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::CommonBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::TextureBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::CombinedTextureSamplerBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::BufferBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::SamplerBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::AccelerationStructureBindingData)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::GenericShaderBinding)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderTextureParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderCombinedTextureSamplerParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderDataParam)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderBufferParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderAccelerationStructureParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::GenericShaderParamEntry)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderDescriptorSet)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::smd::ShaderMetaData)
