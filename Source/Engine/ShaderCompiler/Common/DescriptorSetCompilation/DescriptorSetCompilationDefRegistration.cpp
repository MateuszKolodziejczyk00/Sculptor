#include "DescriptorSetCompilationDefRegistration.h"
#include "DescriptorSetCompilationDefsRegistry.h"

namespace spt::sc
{

DescriptorSetCompilationDefRegistration::DescriptorSetCompilationDefRegistration(const lib::HashedString& dsName, const lib::String& dsCode)
{
	DescriptorSetCompilationDefsRegistry::RegisterDSCompilationDef(dsName, DescriptorSetCompilationDef(dsCode));
}

} // spt::sc
