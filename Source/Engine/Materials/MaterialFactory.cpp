#include "MaterialFactory.h"


namespace spt::mat
{

ecs::EntityHandle MaterialFactory::CreateMaterial(const MaterialDefinition& materialDef, ecs::EntityHandle materialShaderHandle, const MaterialDataParameters& materialData)
{
	const ecs::EntityHandle materialHandle = ecs::CreateEntity();

	MaterialProxyComponent materialProxy;
	materialProxy.materialDataSuballocation = materialData.suballocation;
	materialProxy.params.features           = materialData.features;
	materialProxy.params.customOpacity      = materialDef.customOpacity;
	materialProxy.params.doubleSided        = materialDef.doubleSided;
	materialProxy.params.transparent        = materialDef.transparent;
	materialProxy.params.emissive           = materialDef.emissive;

	materialProxy.params.shader.materialShaderHandle   = materialShaderHandle;
	materialProxy.params.shader.materialDataStructName = materialData.materialDataStructName;

	materialHandle.emplace<MaterialProxyComponent>(materialProxy);

	return materialHandle;
}

} // spt::mat
