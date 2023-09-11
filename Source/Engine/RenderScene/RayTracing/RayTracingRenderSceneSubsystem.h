#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderSceneSubsystem.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Material.h"

namespace spt::rdr
{
class TopLevelAS;
class Buffer;
} // spt::rsc


namespace spt::rsc
{

BEGIN_ALIGNED_SHADER_STRUCT(16, RTInstanceData)
	SHADER_STRUCT_FIELD(Uint32, entityIdx)
	SHADER_STRUCT_FIELD(Uint32, materialDataOffset)
	SHADER_STRUCT_FIELD(Uint32, geometryDataID)
END_SHADER_STRUCT();


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

	Uint32 GetMaterialShaderSBTRecordIdx(mat::MaterialShadersHash materialShadersHash) const;

	const lib::DynamicArray<mat::MaterialShadersHash>& GetMaterialShaderSBTRecords() const;

	Bool IsTLASDirty() const;
	Bool AreSBTRecordsDirty() const;

private:

	void UpdateTLAS();

	lib::SharedPtr<rdr::Buffer> BuildRTInstancesBuffer(const lib::DynamicArray<RTInstanceData>& instances) const;

	lib::SharedPtr<rdr::TopLevelAS> m_tlas;

	lib::SharedPtr<rdr::Buffer> m_rtInstancesDataBuffer;

	lib::HashMap<mat::MaterialShadersHash, Uint32> m_materialShaderToSBTRecordIdx;
	lib::DynamicArray<mat::MaterialShadersHash> m_materialShaderSBTRecords;

	Bool m_isTLASDirty;
	Bool m_areSBTRecordsDirty;
};

} // spt::rsc