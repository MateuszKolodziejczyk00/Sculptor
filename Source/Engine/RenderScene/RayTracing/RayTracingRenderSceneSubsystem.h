#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderSceneSubsystem.h"
#include "ShaderStructs/ShaderStructsMacros.h"
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

	Uint32 GetMaterialShaderSBTRecordIdx(mat::MaterialShadersHash materialShadersHash) const;

	const lib::DynamicArray<mat::MaterialShadersHash>& GetMaterialShaderSBTRecords() const;

	void FillRayTracingGeometryHitGroups(lib::HashedString materialTechnique, INOUT lib::DynamicArray<rdr::RayTracingHitGroup>& hitGroups) const;

	Bool IsTLASDirty() const;
	Bool AreSBTRecordsDirty() const;

private:

	void UpdateTLAS();

	lib::SharedPtr<rdr::Buffer> BuildRTInstancesBuffer(const lib::DynamicArray<RTInstanceData>& instances) const;

	lib::SharedPtr<rdr::TopLevelAS> m_tlas;

	lib::SharedPtr<rdr::Buffer> m_rtInstancesDataBuffer;

	lib::MTHandle<SceneRayTracingDS> m_sceneRayTracingDS;

	lib::HashMap<mat::MaterialShadersHash, Uint32> m_materialShaderToSBTRecordIdx;
	lib::DynamicArray<mat::MaterialShadersHash> m_materialShaderSBTRecords;

	Bool m_isTLASDirty;
	Bool m_areSBTRecordsDirty;
};

} // spt::rsc