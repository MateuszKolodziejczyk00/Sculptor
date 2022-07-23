#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RendererUtils.h"
#include "RHIFwd.h"


namespace spt::rhi
{
struct RHIAllocationInfo;

struct TextureDefinition;
}


namespace spt::renderer
{

class Buffer;
class Texture;


class RENDERER_TYPES_API RendererBuilder
{
public:

	static lib::SharedPtr<Buffer>				CreateBuffer(const RendererResourceName& name, Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo);
	static lib::SharedPtr<Texture>				CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo);
};

}