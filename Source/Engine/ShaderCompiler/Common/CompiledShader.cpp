#include "CompiledShader.h"

namespace spt::sc
{

CompiledShader::CompiledShader()
	: stage(rhi::EShaderStage::None)
{ }

Bool CompiledShader::IsValid() const
{
	return stage != rhi::EShaderStage::None && !binary.empty() && entryPoint.IsValid();
}

void CompiledShader::Serialize(srl::Serializer& serializer)
{
		serializer.Serialize("Binary", binary);
		
		serializer.Serialize("Stage", stage);
		serializer.Serialize("EntryPoint", entryPoint);
			
		serializer.Serialize("MetaData", metaData);
		
#if SPT_SHADERS_DEBUG_FEATURES
		serializer.Serialize("DebugMetaData", debugMetaData);
#endif // SPT_SHADERS_DEBUG_FEATURES

#if WITH_SHADERS_HOT_RELOAD
		serializer.Serialize("FileDependencies", fileDependencies);
#endif // WITH_SHADERS_HOT_RELOAD
}

} // spt::sc
