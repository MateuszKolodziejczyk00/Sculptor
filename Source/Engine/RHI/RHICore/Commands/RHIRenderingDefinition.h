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
	float		depth;
	Uint32		stencil;
};


struct ClearColor
{
	ClearColor()
		: asFloat{ 0.f, 0.f, 0.f, 0.f }
	{ }

	ClearColor(Real32 r, Real32 g, Real32 b, Real32 a)
		: asFloat{ r, g, b, a }
	{ }
	
	ClearColor(Uint32 r, Uint32 g, Uint32 b, Uint32 a)
		: asUint{ r, g, b, a }
	{ }
	
	ClearColor(Int32 r, Int32 g, Int32 b, Int32 a)
		: asInt{ r, g, b, a }
	{ }
	
	explicit ClearColor(Real32 depth, Uint32 stencil = 0)
		: asDepthStencil{ depth, stencil }
	{ }

	union
	{
		Real32				asFloat[4];
		Uint32				asUint[4];
		Int32				asInt[4];
		ClearDepthStencil	asDepthStencil;
	};
};


template<typename TextureViewType = RHITextureView>
struct RTGenericDefinition
{
public:

	RTGenericDefinition()
		: loadOperation(ERTLoadOperation::DontCare)
		, storeOperation(ERTStoreOperation::DontCare)
		, resolveMode(ERTResolveMode::Average)
		, clearColor{ 0.f, 0.f, 0.f, 0.f }
	{ }

	TextureViewType			textureView;
	TextureViewType			resolveTextureView;
	ERTLoadOperation		loadOperation;
	ERTStoreOperation		storeOperation;
	ERTResolveMode			resolveMode;
	ClearColor				clearColor;
};


using RHIRenderTargetDefinition = RTGenericDefinition<>;


enum class ERenderingFlags : Flags32
{
	None							= 0,
	ContentsSecondaryCmdBuffers		= BIT(0),
	Resuming						= BIT(1),

	Default							= None
};


struct RenderingDefinition
{
public:

	RenderingDefinition()
		: renderingFlags(ERenderingFlags::Default)
		, renderAreaOffset(0, 0)
		, renderAreaExtent(0, 0)
	{ }

	ERenderingFlags									renderingFlags;
	math::Vector2i									renderAreaOffset;
	math::Vector2u									renderAreaExtent;
	lib::DynamicArray<RHIRenderTargetDefinition>	colorRTs;
	RHIRenderTargetDefinition						depthRT;
	RHIRenderTargetDefinition						stencilRT;
};

} // spt::rhi