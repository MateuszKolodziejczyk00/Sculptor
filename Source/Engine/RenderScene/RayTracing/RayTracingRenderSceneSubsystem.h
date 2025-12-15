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
#include "RayTracingSceneTypes.h"

namespace spt::rdr
{
class TopLevelAS;
class Buffer;
} // spt::rsc


namespace spt::rsc
{

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
	virtual void UpdateGPUSceneData(RenderSceneConstants& sceneData) override;
	// End RenderSceneSubsystem overrides

	const lib::SharedPtr<rdr::TopLevelAS>& GetSceneTLAS() const { return m_tlas; }

	const lib::SharedPtr<rdr::Buffer>& GetRTInstancesDataBuffer() const;

	Bool IsTLASDirty() const;

private:

	void UpdateTLAS();

	lib::SharedPtr<rdr::Buffer> BuildRTInstancesBuffer(const lib::DynamicArray<RTInstanceData>& instances) const;

	lib::SharedPtr<rdr::TopLevelAS> m_tlas;

	lib::SharedPtr<rdr::Buffer> m_rtInstancesDataBuffer;

	Bool m_isTLASDirty;
	Bool m_areSBTRecordsDirty;
};

} // spt::rsc