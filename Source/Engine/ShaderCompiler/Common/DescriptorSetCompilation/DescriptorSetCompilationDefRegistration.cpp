#include "DescriptorSetCompilationDefRegistration.h"
#include "DescriptorSetCompilationDefsRegistry.h"

namespace spt::sc
{

DescriptorSetCompilationDefRegistration::DescriptorSetCompilationDefRegistration(lib::String dsName, lib::String dsCode, lib::String accessorsCode, const DescriptorSetCompilationMetaData& metaData)
{
	DescriptorSetCompilationDefsRegistry::RegisterDSCompilationDef(std::move(dsName), DescriptorSetCompilationDef(std::move(dsCode), std::move(accessorsCode), metaData));
}

} // spt::sc
