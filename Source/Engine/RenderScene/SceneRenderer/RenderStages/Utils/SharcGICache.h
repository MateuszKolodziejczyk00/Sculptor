#pragma once

#include "SculptorCoreTypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "ShaderStructs/ShaderStructs.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rdr
{
class Buffer;
} // spt::rdr


namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;


BEGIN_SHADER_STRUCT(SharcCacheConstants)
	SHADER_STRUCT_FIELD(Uint32, entriesNum)
END_SHADER_STRUCT();


DS_BEGIN(SharcCacheDS, rg::RGDescriptorSetState<SharcCacheDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint64>),          u_hashEntries)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<math::Vector4u>),  u_voxelData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SharcCacheConstants>), u_sharcCacheConstants)
DS_END();


class SharcGICache
{
public:

	static Bool IsSharcSupported();

	SharcGICache();

	void Update(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

private:

	Uint32 m_entriesNum = 1u << 22;

	lib::SharedPtr<rdr::Buffer> m_hashEntriesBuffer;
	lib::SharedPtr<rdr::Buffer> m_voxelData;
	lib::SharedPtr<rdr::Buffer> m_prevVoxelData;

	lib::SharedPtr<rdr::Buffer> m_copyBuffer;

	Bool m_requresReset = true;
};

} // spt::rsc