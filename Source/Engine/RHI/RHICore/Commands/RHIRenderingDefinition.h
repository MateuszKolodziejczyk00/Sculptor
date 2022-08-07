#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHITextureImpl.h"


namespace spt::rhi
{

enum class ERTLoadOperation
{
	Load,
	Clear,
	DontCare
};


enum class ERTStoreOperation
{
	Store,
	DontCare
};


enum class ERTResolveMode
{
	Average,
	Min,
	Max,
	SampleZero
};


struct ClearDepthStencil
{
	float		m_depth;
	Uint32		m_stencil;
};


struct ClearColor
{
	union
	{
		Real32				m_float[4];
		Uint32				m_uint[4];
		Int32				m_int[4];
		ClearDepthStencil	m_depthStencil;
	};
};


template<typename TextureType = RHITextureView>
struct RTGenericDefinition
{
public:

	RTGenericDefinition()
		: m_loadOperation(ERTLoadOperation::DontCare)
		, m_storeOperation(ERTStoreOperation::DontCare)
		, m_resolveMode(ERTResolveMode::Average)
	{ }

	TextureType				m_textureView;
	TextureType				m_resolveTextureView;
	ERTLoadOperation		m_loadOperation;
	ERTStoreOperation		m_storeOperation;
	ERTResolveMode			m_resolveMode;
	ClearColor				m_clearColor;
};


using RHIRenderTargetDefinition = RTGenericDefinition<>;


namespace ERenderingFlags
{

enum Flags : Flags32
{
	None							= 0,
	ContentsSecondaryCmdBuffers		= BIT(0),
	Resuming						= BIT(1),
};

}


struct RenderingDefinition
{
public:

	RenderingDefinition()
	{ }

	Flags32								m_renderingFlags;
	math::Vector2i						m_renderAreaOffset;
	math::Vector2u						m_renderAreaExtent;
	lib::DynamicArray<RHIRenderTargetDefinition>	m_colorRTs;
	RHIRenderTargetDefinition					m_depthRT;
	RHIRenderTargetDefinition					m_stencilRT;
};

}