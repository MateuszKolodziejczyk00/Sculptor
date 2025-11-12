#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderSceneSubsystem.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Material.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/AccelerationStructureBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "Pipelines/PipelineState.h"

namespace spt::rdr
{
class TopLevelAS;
class Buffer;
} // spt::rsc


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(RTInstanceData)
	SHADER_STRUCT_FIELD(Uint32, entityIdx)
	SHADER_STRUCT_FIELD(Uint32, geometryDataID)
	SHADER_STRUCT_FIELD(Uint16, materialDataID)
	SHADER_STRUCT_FIELD(Uint32, padding)
END_SHADER_STRUCT();


DS_BEGIN(SceneRayTracingDS, rg::RGDescriptorSetState<SceneRayTracingDS>)
	DS_BINDING(BINDING_TYPE(gfx::AccelerationStructureBinding),            u_sceneTLAS)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<RTInstanceData>), u_rtInstances)
DS_END();


enum class ETLASGeometryMask : Uint32
{
	None        = 0u,
	Opaque      = BIT(0u),
	Transparent = BIT(1u)
};


class RENDER_SCENE_API RayTracingRenderSceneSubsystem : public RenderSceneSubsystem
{
protected:

	using Super = RenderSceneSubsystem;

public:

	explicit RayTracingRenderSceneSubsystem(RenderScene& owningScene);

	// Begin RenderSceneSubsystem overrides
	virtual void Update() override;
	// End RenderSceneSubsystem overrides

	const lib::SharedPtr<rdr::TopLevelAS>& GetSceneTLAS() const { return m_tlas; }

	const lib::SharedPtr<rdr::Buffer>& GetRTInstancesDataBuffer() const;

	const lib::MTHandle<SceneRayTracingDS>& GetSceneRayTracingDS() const;

	Bool IsTLASDirty() const;

private:

	void UpdateTLAS();

	lib::SharedPtr<rdr::Buffer> BuildRTInstancesBuffer(const lib::DynamicArray<RTInstanceData>& instances) const;

	lib::SharedPtr<rdr::TopLevelAS> m_tlas;

	lib::SharedPtr<rdr::Buffer> m_rtInstancesDataBuffer;

	lib::MTHandle<SceneRayTracingDS> m_sceneRayTracingDS;

	Bool m_isTLASDirty;
	Bool m_areSBTRecordsDirty;
};

} // spt::rsc