#include "MaterialFactory.h"


namespace spt::mat
{

ecs::EntityHandle MaterialFactory::CreateMaterial(const MaterialDefinition& materialDef, ecs::EntityHandle materialShaderHandle, const MaterialDataParameters& materialData)
{
	const ecs::EntityHandle materialHandle = ecs::CreateEntity();

	MaterialProxyComponent materialProxy;
	materialProxy.materialDataSuballocation			= materialData.suballocation;
	materialProxy.params.materialType				= materialDef.materialType;
	materialProxy.params.materialShaderHandle		= materialShaderHandle;
	materialProxy.params.materialDataStructName		= materialData.materialDataStructName;
	materialProxy.params.customOpacity				= materialDef.customOpacity;

	materialProxy.materialShadersHash				= materialProxy.params.GenerateShaderID();

	materialHandle.emplace<MaterialProxyComponent>(materialProxy);

	return materialHandle;
}

} // spt::mat
