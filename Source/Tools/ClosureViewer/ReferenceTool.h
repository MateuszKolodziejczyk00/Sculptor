#pragma once

#include "SculptorCoreTypes.h"
#include "FileSystem/File.h"
#include "ClosureViewerCommon.h"


namespace spt::closure_viewer::ref_tool
{

constexpr const char* refsDirectoryName    = "Closures/Refs";
constexpr const char* raportsDirectoryName = "Closures/Raports";

Bool CreateReference(const ClosureMaterial& material, const lib::Path& outputDirectory);

Bool GenerateComparisonRaport(const lib::Path& referenceDirectory, const lib::Path& raportPath);

} // spt::closure_viewer::ref_tool
