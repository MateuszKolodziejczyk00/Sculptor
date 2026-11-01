#pragma once

#include "SculptorCoreTypes.h"
#include "FileSystem/File.h"
#include "ClosureViewerCommon.h"


namespace spt::closure_viewer::ref_tool
{

constexpr const char* refsDirectoryName    = "Closures/Refs";
constexpr const char* reportsDirectoryName = "Closures/Reports";

Bool CreateReference(const ClosureMaterial& material, const lib::Path& outputDirectory);

Bool GenerateComparisonReport(const lib::Path& referenceDirectory, const lib::Path& reportPath);

} // spt::closure_viewer::ref_tool
