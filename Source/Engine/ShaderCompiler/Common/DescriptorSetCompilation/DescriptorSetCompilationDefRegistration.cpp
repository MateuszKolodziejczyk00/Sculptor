#include "DescriptorSetCompilationDefRegistration.h"
#include "DescriptorSetCompilationDefsRegistry.h"

namespace spt::sc
{

DescriptorSetCompilationDefRegistration::DescriptorSetCompilationDefRegistration(const lib::HashedString& dsName, const lib::String& dsCode, const DescriptorSetCompilationMetaData& metaData)
{
	DescriptorSetCompilationDefsRegistry::RegisterDSCompilationDef(dsName, DescriptorSetCompilationDef(dsCode, metaData));
}

} // spt::sc
