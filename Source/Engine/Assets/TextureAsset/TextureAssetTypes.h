#pragma once

#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "RHI/RHICore/RHITextureTypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::as
{


struct TextureDefinition
{
	math::Vector3u       resolution;
	rhi::EFragmentFormat format;
	Uint32               mipLevelsNum;
};
SPT_REGISTER_ASSET_DATA_TYPE(TextureDefinition);


struct TextureMipData
{
	lib::DynamicArray<Byte> data;
};


struct TextureData
{
	lib::DynamicArray<TextureMipData> mips;
};
SPT_REGISTER_ASSET_DATA_TYPE(TextureData);

} // spt::as


namespace spt::srl
{

template<>
struct TypeSerializer<as::TextureDefinition>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("Resolution", data.resolution);
		serializer.Serialize("MipLevelsNum", data.mipLevelsNum);
		if constexpr (Serializer::IsSaving())
		{
			serializer.Serialize("Format", rhi::GetFormatName(data.format));
		}
		else
		{
			lib::String formatName;
			serializer.Serialize("Format", formatName);
			data.format = rhi::GetFormatByName(formatName);
		}
	}
};

template<>
struct TypeSerializer<as::TextureMipData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		if constexpr (Serializer::IsSaving())
		{
			const srl::Binary binary(reinterpret_cast<const unsigned char*>(data.data.data()), data.data.size());
			serializer.Serialize("Data", binary);
		}
		else
		{
			srl::Binary binary;
			serializer.Serialize("Data", binary);
			data.data.resize(binary.size());
			memcpy_s(data.data.data(), data.data.size(), binary.data(), binary.size());
		}

	}
};

template<>
struct TypeSerializer<as::TextureData>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("Mips", data.mips);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::as::TextureDefinition);
SPT_YAML_SERIALIZATION_TEMPLATES(spt::as::TextureMipData);
SPT_YAML_SERIALIZATION_TEMPLATES(spt::as::TextureData);
