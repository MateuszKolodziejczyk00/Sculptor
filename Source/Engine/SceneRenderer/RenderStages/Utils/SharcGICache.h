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
struct SceneRendererInterface;


BEGIN_SHADER_STRUCT(SharcCacheConstants)
	SHADER_STRUCT_FIELD(Uint32, entriesNum)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SharcShadersPermutation)
	SHADER_STRUCT_FIELD(Bool, SHARC_DEMODULATE_MATERIALS)
END_SHADER_STRUCT()


DS_BEGIN(SharcCacheDS, rg::RGDescriptorSetState<SharcCacheDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint64>),          u_hashEntries)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<math::Vector4u>),  u_voxelData)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<SharcCacheConstants>), u_sharcCacheConstants)
DS_END();


struct SharcUpdateParams
{
	Bool   resetCache = false;
	Uint32 ssrStepsNum = 0;
	Real32 ssrTraceLength = 0.f;
};


class SharcGICache
{
public:

	static Bool IsSharcSupported();
	static Bool IsDemodulatingMaterials();

	static SharcShadersPermutation GetShadersPermutation()
	{
		SharcShadersPermutation permutation;
		permutation.SHARC_DEMODULATE_MATERIALS = IsDemodulatingMaterials();
		return permutation;
	}

	SharcGICache();

	void Update(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SharcUpdateParams& params);

private:

	Uint32 m_entriesNum = 1u << 22;

	lib::SharedPtr<rdr::Buffer> m_hashEntriesBuffer;
	lib::SharedPtr<rdr::Buffer> m_voxelData;
	lib::SharedPtr<rdr::Buffer> m_prevVoxelData;

	lib::SharedPtr<rdr::Buffer> m_copyBuffer;

	Bool m_requresReset = true;

	Bool m_isDemodulatingMaterials = false;
};

} // spt::rsc
