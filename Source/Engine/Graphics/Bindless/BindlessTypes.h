#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "Types/Texture.h"
#include "Types/Buffer.h"
#include "RGResources/RGResourceHandles.h"
#include "DependenciesBuilder.h"
#include "Types/AccelerationStructure.h"


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

	TextureDescriptor(const lib::SharedPtr<rdr::TextureView>& texture)
		: m_textureView(texture)
	{ }

	TextureDescriptor(const rg::RGTextureViewHandle& texture)
		: m_textureView(texture)
	{ }

	TextureDescriptor(const TextureDescriptor& other) = default;
	TextureDescriptor& operator=(const TextureDescriptor& other) = default;
	TextureDescriptor(TextureDescriptor&& other) = default;
	TextureDescriptor& operator=(TextureDescriptor&& other) = default;

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


struct BufferDescriptorMetadata
{
	bool isRW       = false;
	bool isUAV      = false;
	bool isOptional = false;
	bool isConst    = false;
};


#if DO_CHECKS
GRAPHICS_API void ValidateBufferBinding(const std::variant<lib::SharedPtr<rdr::BindableBufferView>, rg::RGBufferViewHandle>& boundView, BufferDescriptorMetadata metadata);
#endif // DO_CHECKS


template<BufferDescriptorMetadata metadata, typename TType>
class BufferDescriptor
{
public:

	static constexpr BufferDescriptorMetadata GetMetadata() { return metadata; }

	BufferDescriptor() = default;


	BufferDescriptor& operator=(lib::SharedPtr<rdr::BindableBufferView> buffer)
	{
		m_bufferView = std::move(buffer);
		return *this;
	}

	BufferDescriptor& operator=(rg::RGBufferViewHandle buffer)
	{
		m_bufferView = std::move(buffer);
		return *this;
	}

	Uint32 GetDescriptorIdx() const
	{
#if DO_CHECKS
		ValidateBufferBinding(m_bufferView, metadata);
#endif // DO_CHECKS

		if (std::holds_alternative<lib::SharedPtr<rdr::BindableBufferView>>(m_bufferView))
		{
			const lib::SharedPtr<rdr::BindableBufferView>& bufferView = std::get<lib::SharedPtr<rdr::BindableBufferView>>(m_bufferView);

			if constexpr (metadata.isOptional)
			{
				if (!bufferView)
				{
					return rdr::invalidResourceDescriptorIdx;
				}
			}
			else
			{
				SPT_CHECK_MSG(bufferView, "Invalid buffer view in BufferDescriptor");
			}

			if constexpr (metadata.isUAV)
			{
				return bufferView->GetUAVDescriptor();
			}
			else
			{
				SPT_CHECK_NO_ENTRY_MSG("Not implemented");
				return rdr::invalidResourceDescriptorIdx;
			}
		}
		else
		{
			const rg::RGBufferViewHandle& bufferView = std::get<rg::RGBufferViewHandle>(m_bufferView);

			if constexpr (metadata.isOptional)
			{
				if (!bufferView.IsValid())
				{
					return rdr::invalidResourceDescriptorIdx;
				}
			}
			else
			{
				SPT_CHECK_MSG(bufferView, "Invalid buffer view in BufferDescriptor");
			}

			if constexpr (metadata.isUAV)
			{
				return bufferView->GetUAVDescriptorChecked();
			}
			else
			{
				SPT_CHECK_NO_ENTRY_MSG("Not implemented");
				return rdr::invalidResourceDescriptorIdx;
			}
		}
	}

	Bool IsValid() const
	{
		if (std::holds_alternative<rg::RGBufferViewHandle>(m_bufferView))
		{
			return std::get<rg::RGBufferViewHandle>(m_bufferView).IsValid();
		}
		else
		{
			return !!std::get<lib::SharedPtr<rdr::BindableBufferView>>(m_bufferView);
		}
	}

private:

	std::variant<lib::SharedPtr<rdr::BindableBufferView>, rg::RGBufferViewHandle> m_bufferView;
};


using ByteBuffer         = BufferDescriptor<BufferDescriptorMetadata{ false, true, true, false },  Byte>;
using RWByteBuffer       = BufferDescriptor<BufferDescriptorMetadata{ true, true, true, false },   Byte>;
using ByteBufferRef      = BufferDescriptor<BufferDescriptorMetadata{ false, true, false, false }, Byte>;
using RWByteBufferRef    = BufferDescriptor<BufferDescriptorMetadata{ true, true, false, false },  Byte>;
using ConstByteBuffer    = BufferDescriptor<BufferDescriptorMetadata{ false, true, true, true },   Byte>;
using ConstByteBufferRef = BufferDescriptor<BufferDescriptorMetadata{ false, true, false, true },  Byte>;


template<typename TType>
using TypedBuffer         = BufferDescriptor<BufferDescriptorMetadata{ false, true, true, false }, TType>;
template<typename TType>
using RWTypedBuffer       = BufferDescriptor<BufferDescriptorMetadata{ true, true, true, false }, TType>;
template<typename TType>
using TypedBufferRef      = BufferDescriptor<BufferDescriptorMetadata{ false, true, false, false }, TType>;
template<typename TType>
using RWTypedBufferRef    = BufferDescriptor<BufferDescriptorMetadata{ true, true, false, false }, TType>;
template<typename TType>
using ConstTypedBuffer    = BufferDescriptor<BufferDescriptorMetadata{ false, true, true, true }, TType>;
template<typename TType>
using ConstTypedBufferRef = BufferDescriptor<BufferDescriptorMetadata{ false, true, false, true }, TType>;


struct TLASDescriptorMetadata
{
	Bool isOptional = false;
};


#if DO_CHECKS
GRAPHICS_API void ValidateTLASBinding(const lib::SharedPtr<rdr::TopLevelAS>& boundTLAS, TLASDescriptorMetadata metadata);
#endif // DO_CHECKS


template<TLASDescriptorMetadata metadata>
class TLASDescriptor
{
public:

	static constexpr TLASDescriptorMetadata GetMetadata() { return metadata; }

	TLASDescriptor() = default;

	TLASDescriptor& operator=(lib::SharedPtr<rdr::TopLevelAS> tlas)
	{
		m_tlas = std::move(tlas);
		return *this;
	}

	Uint32 GetDescriptorIdx() const
	{
		SPT_CHECK_MSG(m_tlas, "Invalid TLAS in TLASDescriptor");
		return m_tlas->GetSRVDescriptor();
	}

	Bool IsValid() const
	{
		return !!m_tlas;
	}

private:

	lib::SharedPtr<rdr::TopLevelAS> m_tlas;
};


using TLASRef = TLASDescriptor<TLASDescriptorMetadata{ false }>;
using TLAS    = TLASDescriptor<TLASDescriptorMetadata{ true }>;


template<typename TNamedBuffer, typename TDataType>
class GPUNamedElemPtr
{
public:

	GPUNamedElemPtr() = default;

	explicit GPUNamedElemPtr(Uint32 offset)
		: m_bufferElementIdx(OffsetToIndex(offset))
	{
	}

	GPUNamedElemPtr(const GPUNamedElemPtr& rhs) = default;
	GPUNamedElemPtr& operator=(const GPUNamedElemPtr& rhs) = default;

	Bool IsValid() const
	{
		return m_bufferElementIdx != idxNone<Uint32>;
	}

	Uint32 GetOffset() const
	{
		SPT_CHECK(IsValid());
		return m_bufferElementIdx * sizeof(rdr::HLSLStorage<TDataType>);
	}

	Uint32 GetBufferElementIdx() const
	{
		return m_bufferElementIdx;
	}

	GPUNamedElemPtr operator+(Uint32 delta) const
	{
		SPT_CHECK(IsValid());
		return GPUNamedElemPtr((GetBufferElementIdx() + delta) * sizeof(rdr::HLSLStorage<TDataType>));
	}

	static constexpr lib::String GetTypeName()
	{
		return lib::String("GPUNamedElemPtr<Accessor_") + TNamedBuffer::GetName() + ", " + rdr::shader_translator::GetTypeName<TDataType>() + ">";
	}

private:

	Uint32 OffsetToIndex(Uint32 offset) const
	{
		SPT_CHECK(offset % sizeof(rdr::HLSLStorage<TDataType>) == 0u);
		return offset / sizeof(rdr::HLSLStorage<TDataType>);
	}
	
	Uint32 m_bufferElementIdx = idxNone<Uint32>;
};


template<typename TNamedBuffer, typename TDataType>
class GPUNamedElemsSpan
{
public:

	GPUNamedElemsSpan() = default;

	GPUNamedElemsSpan(Uint32 offset, Uint32 size)
		: m_firstElemIdx(OffsetToIndex(offset))
		, m_size(size)
	{ }

	GPUNamedElemsSpan(const GPUNamedElemsSpan& rhs) = default;
	GPUNamedElemsSpan& operator=(const GPUNamedElemsSpan& rhs) = default;

	Bool IsValid() const
	{
		return m_firstElemIdx != idxNone<Uint32> && m_size > 0;
	}

	Uint32 GetOffset() const
	{
		SPT_CHECK(IsValid());
		return m_firstElemIdx * sizeof(rdr::HLSLStorage<TDataType>);
	}

	Uint32 GetFirstElemIdx() const
	{
		return m_firstElemIdx;
	}

	Uint32 GetSize() const
	{
		return m_size;
	}

	static constexpr lib::String GetTypeName()
	{
		return lib::String("GPUNamedElemsSpan<Accessor_") + TNamedBuffer::GetName() + ", " + rdr::shader_translator::GetTypeName<TDataType>() + ">";
	}

	GPUNamedElemPtr<TNamedBuffer, TDataType> GetElemPtr(Uint32 localIdx) const
	{
		SPT_CHECK(localIdx < m_size);
		return GPUNamedElemPtr<TNamedBuffer, TDataType>(m_firstElemIdx + localIdx);
	}

private:

	Uint32 OffsetToIndex(Uint32 offset) const
	{
		SPT_CHECK(offset % sizeof(rdr::HLSLStorage<TDataType>) == 0u);
		return offset / sizeof(rdr::HLSLStorage<TDataType>);
	}
	
	Uint32 m_firstElemIdx = idxNone<Uint32>;
	Uint32 m_size         = 0u;
};

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
		SPT_CHECK(hlslData.size() == 8u);
		Uint32* hlsl = reinterpret_cast<Uint32*>(hlslData.data());
		hlsl[0] = cppData.GetDescriptorIdx();
		hlsl[1] = 2137;
	}
};


template<gfx::TextureDescriptorMetadata metadata, typename TType>
struct StructHLSLSizeEvaluator<gfx::TextureDescriptor<metadata, TType>>
{
	static constexpr Uint32 Size()
	{
		return 8u;
	}
};


template<gfx::TextureDescriptorMetadata metadata, typename TType>
struct StructHLSLAlignmentEvaluator<gfx::TextureDescriptor<metadata, TType>>
{
	static constexpr Uint32 Alignment()
	{
		return 4u;
	}
};


template<gfx::BufferDescriptorMetadata metadata, typename TType>
struct StructTranslator<gfx::BufferDescriptor<metadata, TType>>
{
	static constexpr lib::String GetHLSLStructName()
	{
		if constexpr (metadata.isUAV)
		{
			const lib::String prefix = metadata.isRW ? "RW" : "";
			if constexpr (std::is_same_v<TType, Byte>)
			{
				return prefix + "ByteBuffer";
			}
			else
			{
				if constexpr (CShaderStruct<TType>)
				{
					// Can't use StructTranslator here, as specialization might be not defined at this point for TType
					return prefix + "TypedBuffer" + "<" + TType::GetStructName() + ">";
				}
				else
				{
					return prefix + "TypedBuffer" + "<" + StructTranslator<TType>::GetHLSLStructName() + ">";
				}
			}
		}
		else
		{
			SPT_CHECK_MSG(false, "For now, only UAV buffers are supported")
		}
	}
};


template<gfx::BufferDescriptorMetadata metadata, typename TType>
struct StructCPPToHLSLTranslator<gfx::BufferDescriptor<metadata, TType>>
{
	static void Copy(const gfx::BufferDescriptor<metadata, TType>& cppData, lib::Span<Byte> hlslData)
	{
		SPT_CHECK(hlslData.size() == 8u);
		Uint32* hlsl = reinterpret_cast<Uint32*>(hlslData.data());
		hlsl[0] = cppData.GetDescriptorIdx();
		hlsl[1] = 2137;
	}
};


template<gfx::BufferDescriptorMetadata metadata, typename TType>
struct StructHLSLSizeEvaluator<gfx::BufferDescriptor<metadata, TType>>
{
	static constexpr Uint32 Size()
	{
		return 8u;
	}
};


template<gfx::BufferDescriptorMetadata metadata, typename TType>
struct StructHLSLAlignmentEvaluator<gfx::BufferDescriptor<metadata, TType>>
{
	static constexpr Uint32 Alignment()
	{
		return 4u;
	}
};


template<gfx::BufferDescriptorMetadata metadata, typename TType>
struct ShaderStructReferencer<gfx::BufferDescriptor<metadata, TType>>
{
	static constexpr void CollectReferencedStructs(lib::DynamicArray<lib::String>& references)
	{
		ShaderStructReferencer<TType>::CollectReferencedStructs(references);
	}
};


template<gfx::TLASDescriptorMetadata metadata>
struct StructTranslator<gfx::TLASDescriptor<metadata>>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return "TLAS";
	}
};


template<gfx::TLASDescriptorMetadata metadata>
struct StructCPPToHLSLTranslator<gfx::TLASDescriptor<metadata>>
{
	static void Copy(const gfx::TLASDescriptor<metadata>& cppData, lib::Span<Byte> hlslData)
	{
		SPT_CHECK(hlslData.size() == 8u);
		Uint32* hlsl = reinterpret_cast<Uint32*>(hlslData.data());
		hlsl[0] = cppData.GetDescriptorIdx();
		hlsl[1] = 2137;
	}
};


template<gfx::TLASDescriptorMetadata metadata>
struct StructHLSLSizeEvaluator<gfx::TLASDescriptor<metadata>>
{
	static constexpr Uint32 Size()
	{
		return 8u;
	}
};


template<gfx::TLASDescriptorMetadata metadata>
struct StructHLSLAlignmentEvaluator<gfx::TLASDescriptor<metadata>>
{
	static constexpr Uint32 Alignment()
	{
		return 4u;
	}
};


template<typename TNamedBuffer, typename TDataType>
struct StructTranslator<gfx::GPUNamedElemPtr<TNamedBuffer, TDataType>>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return gfx::GPUNamedElemPtr<TNamedBuffer, TDataType>::GetTypeName();
	}
};


template<typename TNamedBuffer, typename TDataType>
struct StructCPPToHLSLTranslator<gfx::GPUNamedElemPtr<TNamedBuffer, TDataType>>
{
	static void Copy(const gfx::GPUNamedElemPtr<TNamedBuffer, TDataType>& cppData, lib::Span<Byte> hlslData)
	{
		StructCPPToHLSLTranslator<Uint32>::Copy(cppData.GetBufferElementIdx(), hlslData);
	}
};


template<typename TNamedBuffer, typename TDataType>
struct StructHLSLSizeEvaluator<gfx::GPUNamedElemPtr<TNamedBuffer, TDataType>>
{
	static constexpr Uint32 Size()
	{
		return StructHLSLSizeEvaluator<Uint32>::Size();
	}
};


template<typename TNamedBuffer, typename TDataType>
struct StructHLSLAlignmentEvaluator<gfx::GPUNamedElemPtr<TNamedBuffer, TDataType>>
{
	static constexpr Uint32 Alignment()
	{
		return StructHLSLAlignmentEvaluator<Uint32>::Alignment();
	}
};


template<typename TNamedBuffer, typename TDataType>
struct ShaderStructReferencer<gfx::GPUNamedElemPtr<TNamedBuffer, TDataType>>
{
	static constexpr void CollectReferencedStructs(lib::DynamicArray<lib::String>& references)
	{
		ShaderStructReferencer<TDataType>::CollectReferencedStructs(references);
	}
};


template<typename TNamedBuffer, typename TDataType>
struct StructTranslator<gfx::GPUNamedElemsSpan<TNamedBuffer, TDataType>>
{
	static constexpr lib::String GetHLSLStructName()
	{
		return gfx::GPUNamedElemsSpan<TNamedBuffer, TDataType>::GetTypeName();
	}
};


template<typename TNamedBuffer, typename TDataType>
struct StructCPPToHLSLTranslator<gfx::GPUNamedElemsSpan<TNamedBuffer, TDataType>>
{
	static void Copy(const gfx::GPUNamedElemsSpan<TNamedBuffer, TDataType>& cppData, lib::Span<Byte> hlslData)
	{
		StructCPPToHLSLTranslator<Uint32>::Copy(cppData.GetFirstElemIdx(), hlslData.subspan(0u, 4u));
		StructCPPToHLSLTranslator<Uint32>::Copy(cppData.GetSize(), hlslData.subspan(4u, 4u));
	}
};


template<typename TNamedBuffer, typename TDataType>
struct StructHLSLSizeEvaluator<gfx::GPUNamedElemsSpan<TNamedBuffer, TDataType>>
{
	static constexpr Uint32 Size()
	{
		return 8u;
	}
};


template<typename TNamedBuffer, typename TDataType>
struct StructHLSLAlignmentEvaluator<gfx::GPUNamedElemsSpan<TNamedBuffer, TDataType>>
{
	static constexpr Uint32 Alignment()
	{
		return 4u;
	}
};


template<typename TNamedBuffer, typename TDataType>
struct ShaderStructReferencer<gfx::GPUNamedElemsSpan<TNamedBuffer, TDataType>>
{
	static constexpr void CollectReferencedStructs(lib::DynamicArray<lib::String>& references)
	{
		ShaderStructReferencer<TDataType>::CollectReferencedStructs(references);
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
			SPT_CHECK(hlsl.size() == 8u);

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

template<gfx::BufferDescriptorMetadata metadata, typename TType>
struct HLSLStructDependenciesBuider<gfx::BufferDescriptor<metadata, TType>>
{
	static void CollectDependencies(lib::Span<const Byte> hlsl, RGDependenciesBuilder& dependenciesBuilder)
	{
		if constexpr (!metadata.isConst)
		{
			SPT_CHECK(hlsl.size() == 8u);

			const Uint32* hlslData = reinterpret_cast<const Uint32*>(hlsl.data());

			const rdr::ResourceDescriptorIdx descriptoridx = rdr::ResourceDescriptorIdx(hlslData[0]);

			if (descriptoridx != rdr::invalidResourceDescriptorIdx)
			{
				constexpr rg::ERGBufferAccess access = metadata.isRW ? rg::ERGBufferAccess::ReadWrite : rg::ERGBufferAccess::Read;
				rg::RGBufferAccessInfo accessInfo;
				accessInfo.access = access;

#if DEBUG_RENDER_GRAPH
				if constexpr (!std::is_same_v<TType, Byte>)
				{
					accessInfo.structTypeName = rdr::shader_translator::GetTypeName<TType>();
					accessInfo.elementsNum    = idxNone<Uint32>;
				}
#endif // DEBUG_RENDER_GRAPH

				dependenciesBuilder.AddBufferAccess(descriptoridx, accessInfo);
			}
		}
	}
};

} // spt::rg
