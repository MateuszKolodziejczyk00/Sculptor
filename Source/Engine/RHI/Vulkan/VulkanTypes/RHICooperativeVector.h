#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHICooperativeVectorTypes.h"


namespace spt::vulkan
{

class RHI_API RHICooperativeVector
{
public:

	static SizeType ComputeConvertedMatrixSize(const rhi::ConvertCooperativeMatrixDefinition& definition);
	static void     ConvertMatrix(const rhi::ConvertCooperativeMatrixDefinition& definition);
};

} // spt::vulkan
