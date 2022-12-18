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