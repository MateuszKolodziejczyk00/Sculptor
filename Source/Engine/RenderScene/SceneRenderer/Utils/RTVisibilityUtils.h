#pragma once

#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "DescriptorSetBindings/ChildDSBinding.h"
#include "MaterialsUnifiedData.h"
#include "StaticMeshes/StaticMeshGeometry.h"
#include "GeometryManager.h"


namespace spt::rsc
{

DS_BEGIN(RTVisibilityDS, rg::RGDescriptorSetState<RTVisibilityDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<RTInstanceData>),	u_rtInstances)
	DS_BINDING(BINDING_TYPE(gfx::ChildDSBinding<GeometryDS>),				u_geometryDS)
	DS_BINDING(BINDING_TYPE(gfx::ChildDSBinding<StaticMeshUnifiedDataDS>),	u_staticMeshUnifiedDataDS)
	DS_BINDING(BINDING_TYPE(gfx::ChildDSBinding<mat::MaterialsDS>),			u_MaterialsDS)
DS_END();

} // spt::rsc