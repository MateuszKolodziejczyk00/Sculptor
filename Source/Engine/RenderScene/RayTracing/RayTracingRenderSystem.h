#pragma once

#include "RenderSceneMacros.h"
#include "SceneRenderSystem.h"
#include "DescriptorSetBindings/AccelerationStructureBinding.h"
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


class RENDER_SCENE_API RayTracingRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	explicit RayTracingRenderSystem(RenderScene& owningScene);

	// Begin RenderSceneSubsystem overrides
	virtual void Update(const SceneUpdateContext& context) override;
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
