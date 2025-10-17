#include "MaterialFactory.h"


namespace spt::mat
{

ecs::EntityHandle MaterialFactory::CreateMaterial(const MaterialDefinition& materialDef, ecs::EntityHandle materialShaderHandle, const MaterialDataParameters& materialData)
{
	const ecs::EntityHandle materialHandle = ecs::CreateEntity();

	MaterialProxyComponent materialProxy;
	materialProxy.materialDataSuballocation     = materialData.suballocation;
	materialProxy.params.materialShaderHandle   = materialShaderHandle;
	materialProxy.params.materialDataStructName = materialData.materialDataStructName;
	materialProxy.params.customOpacity          = materialDef.customOpacity;
	materialProxy.params.doubleSided            = materialDef.doubleSided;
	materialProxy.params.transparent            = materialDef.transparent;

	materialProxy.materialShadersHash				= materialProxy.params.GenerateShaderID();

	materialHandle.emplace<MaterialProxyComponent>(materialProxy);

	return materialHandle;
}

} // spt::mat
