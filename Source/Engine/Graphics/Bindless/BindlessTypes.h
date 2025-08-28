#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "Types/Texture.h"
#include "RGResources/RGResourceHandles.h"
#include "DependenciesBuilder.h"


namespace spt::rg
{
class RGDependenciesBuilder;
} // spt::rg


namespace spt::gfx
{

struct TextureDescriptorMetadata
{
	bool  isUAV      = false;
	bool  isOptional = false;
	bool  isConst    = false;
	Int32 dimentions = 2;
};


#if DO_CHECKS
GRAPHICS_API void ValidateTextureBinding(const std::variant<lib::SharedPtr<rdr::TextureView>, rg::RGTextureViewHandle>& boundView, TextureDescriptorMetadata metadata);
#endif // DO_CHECKS


template<TextureDescriptorMetadata metadata, typename TType>
class TextureDescriptor
{
public:

	static constexpr TextureDescriptorMetadata GetMetadata() { return metadata; }

	TextureDescriptor() = default;

	TextureDescriptor& operator=(lib::SharedPtr<rdr::TextureView> texture)
	{
		m_textureView = std::move(texture);


		return *this;
	}

	TextureDescriptor& operator=(rg::RGTextureViewHandle texture)
	{
		m_textureView = std::move(texture);

		return *this;
	}

	Uint32 GetDescriptorIdx() const
	{
#if DO_CHECKS
		ValidateTextureBinding(m_textureView, metadata);
#endif // DO_CHECKS

		if (std::holds_alternative<lib::SharedPtr<rdr::TextureView>>(m_textureView))
		{
			const lib::SharedPtr<rdr::TextureView>& textureView = std::get<lib::SharedPtr<rdr::TextureView>>(m_textureView);

			if constexpr (metadata.isOptional)
			{
				if (!textureView)
				{
					return rdr::invalidResourceDescriptorIdx;
				}
			}
			else
			{
				SPT_CHECK_MSG(textureView, "Invalid texture view in TextureDescriptor");
			}

			if constexpr (metadata.isUAV)
			{
				return textureView->GetUAVDescriptor();
			}
			else
			{
				return textureView->GetSRVDescriptor();
			}
		}
		else
		{
			const rg::RGTextureViewHandle& textureView = std::get<rg::RGTextureViewHandle>(m_textureView);

			if constexpr (metadata.isOptional)
			{
				if (!textureView.IsValid())
				{
					return rdr::invalidResourceDescriptorIdx;
				}
			}
			else
			{
				SPT_CHECK_MSG(textureView.IsValid(), "Invalid texture view in TextureDescriptor");
			}

			if constexpr (metadata.isUAV)
			{
				return textureView->GetUAVDescriptorChecked();
			}
			else
			{
				return textureView->GetSRVDescriptorChecked();
			}
		}
	}

	Bool IsValid() const
	{
		if (std::holds_alternative<lib::SharedPtr<rdr::TextureView>>(m_textureView))
		{
			return !!std::get<lib::SharedPtr<rdr::TextureView>>(m_textureView);
		}
		else
		{
			return std::get<rg::RGTextureViewHandle>(m_textureView).IsValid();
		}
	}

private:

	std::variant<lib::SharedPtr<rdr::TextureView>, rg::RGTextureViewHandle> m_textureView;
};


/**
 * Nomenclature:
 * SRV - Shader Resource View (read-only texture)
 * UAV - Unordered Access View (read-write texture)
 * Ref - must be valid (resource is guaranteed to be bound)
 * Const - this means that resource is generally read-only across the frame and doesn't need to be tracked by frame graph
 */


template<Int32 Dims, typename TType>
using SRVTexture         = TextureDescriptor<TextureDescriptorMetadata(false, true,  false, Dims), TType>;

template<Int32 Dims, typename TType>
using SRVTextureRef      = TextureDescriptor<TextureDescriptorMetadata(false, false, false, Dims), TType>;

template<Int32 Dims, typename TType>
using ConstSRVTexture    = TextureDescriptor<TextureDescriptorMetadata(false, true,  true,  Dims), TType>;

template<Int32 Dims, typename TType>
using ConstSRVTextureRef = TextureDescriptor<TextureDescriptorMetadata(false, false, true,  Dims), TType>;

template<Int32 Dims, typename TType>
using UAVTexture         = TextureDescriptor<TextureDescriptorMetadata(true,  true,  false, Dims), TType>;

template<Int32 Dims, typename TType>
using UAVTextureRef      = TextureDescriptor<TextureDescriptorMetadata(true,  false, false, Dims), TType>;


#define SPT_CREATE_TEXTURE_DESCRIPTOR_TYPEDEFS(texType) \
template<typename TType> \
using texType##2D  = texType<2, TType>; \
template<typename TType> \
using texType##3D  = texType<3, TType>; \
template<typename TType> \
using texType##2DRef = texType##Ref<2, TType>; \
template<typename TType> \
using texType##3DRef = texType##Ref<3, TType>; \


SPT_CREATE_TEXTURE_DESCRIPTOR_TYPEDEFS(SRVTexture)
SPT_CREATE_TEXTURE_DESCRIPTOR_TYPEDEFS(ConstSRVTexture)
SPT_CREATE_TEXTURE_DESCRIPTOR_TYPEDEFS(UAVTexture)

} // spt::gfx

namespace spt::rdr::shader_translator
{

template<gfx::TextureDescriptorMetadata metadata, typename TType>
struct StructTranslator<gfx::TextureDescriptor<metadata, TType>>
{
	static constexpr lib::String GetHLSLStructName()
	{
		lib::String dims;
		if constexpr (metadata.dimentions == 1)
		{
			dims = "1D";
		}
		else if constexpr (metadata.dimentions == 2)
		{
			dims = "2D";
		}
		else if constexpr (metadata.dimentions == 3)
		{
			dims = "3D";
		}

		if constexpr (metadata.isUAV)
		{
			return lib::String("UAVTexture") + dims
				+ "<" + StructTranslator<TType>::GetHLSLStructName() + ">";
		}
		else
		{
			return lib::String("SRVTexture") + dims
				+ "<" + StructTranslator<TType>::GetHLSLStructName() + ">";
		}
	}
};


template<gfx::TextureDescriptorMetadata metadata, typename TType>
struct StructCPPToHLSLTranslator<gfx::TextureDescriptor<metadata, TType>>
{
	static void Copy(const gfx::TextureDescriptor<metadata, TType>& cppData, lib::Span<Byte> hlslData)
	{
		SPT_CHECK(hlslData.size() == 16u);
		Uint32* hlsl = reinterpret_cast<Uint32*>(hlslData.data());
		hlsl[0] = cppData.GetDescriptorIdx();
		hlsl[1] = 2137;
		hlsl[2] = 2138;
		hlsl[3] = 2139;
	}
};


template<gfx::TextureDescriptorMetadata metadata, typename TType>
struct StructHLSLSizeEvaluator<gfx::TextureDescriptor<metadata, TType>>
{
	static constexpr Uint32 Size()
	{
		return 16u;
	}
};


template<gfx::TextureDescriptorMetadata metadata, typename TType>
struct StructHLSLAlignmentEvaluator<gfx::TextureDescriptor<metadata, TType>>
{
	static constexpr Uint32 Alignment()
	{
		return 16u;
	}
};

} // spt::rdr::shader_translator

namespace spt::rg
{

template<gfx::TextureDescriptorMetadata metadata, typename TType>
struct HLSLStructDependenciesBuider<gfx::TextureDescriptor<metadata, TType>>
{
	static void CollectDependencies(lib::Span<const Byte> hlsl, RGDependenciesBuilder& dependenciesBuilder)
	{
		if constexpr (!metadata.isConst)
		{
			SPT_CHECK(hlsl.size() == 16u);

			const Uint32* hlslData = reinterpret_cast<const Uint32*>(hlsl.data());

			const rdr::ResourceDescriptorIdx descriptoridx = rdr::ResourceDescriptorIdx(hlslData[0]);

			if (descriptoridx != rdr::invalidResourceDescriptorIdx)
			{
				constexpr rg::ERGTextureAccess access = metadata.isUAV ? ERGTextureAccess::StorageWriteTexture : ERGTextureAccess::SampledTexture;

				dependenciesBuilder.AddTextureAccess(descriptoridx, access);
			}
		}
	}
};

} // spt::rg
