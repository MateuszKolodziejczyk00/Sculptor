#include "DescriptorSetCompilationDefRegistration.h"
#include "DescriptorSetCompilationDefsRegistry.h"

namespace spt::sc
{

DescriptorSetCompilationDefRegistration::DescriptorSetCompilationDefRegistration(const lib::HashedString& dsName, lib::String dsCode, lib::String accessorsCode, const DescriptorSetCompilationMetaData& metaData)
{
	DescriptorSetCompilationDefsRegistry::RegisterDSCompilationDef(dsName, DescriptorSetCompilationDef(std::move(dsCode), std::move(accessorsCode), metaData));
}

} // spt::sc
