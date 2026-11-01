#include "RHICooperativeVector.h"
#include "Vulkan//VulkanRHI.h"
#include "Vulkan/VulkanRHIUtils.h"


namespace spt::vulkan
{

SizeType RHICooperativeVector::ComputeConvertedMatrixSize(const rhi::ConvertCooperativeMatrixDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(definition.srcData.size() > 0);

	SizeType dstSize = 0u;

	VkConvertCooperativeVectorMatrixInfoNV convertInfo{ VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV };
	convertInfo.srcData.hostAddress = nullptr;
	convertInfo.dstData.hostAddress = nullptr;
	convertInfo.srcSize             = definition.srcData.size();
	convertInfo.pDstSize            = &dstSize;
	convertInfo.srcComponentType    = RHIToVulkan::GetCoomponentType(definition.srcType);
	convertInfo.dstComponentType    = RHIToVulkan::GetCoomponentType(definition.dstType);
	convertInfo.srcLayout           = RHIToVulkan::GetCooperativeVectorLayout(definition.srcLayout);
	convertInfo.dstLayout           = RHIToVulkan::GetCooperativeVectorLayout(definition.dstLayout);
	convertInfo.numRows             = definition.numRows;
	convertInfo.numColumns          = definition.numColumns;
	convertInfo.srcStride           = definition.srcStride;
	convertInfo.dstStride           = definition.dstStride;

	SPT_VK_CHECK(vkConvertCooperativeVectorMatrixNV(VulkanRHI::GetDeviceHandle(), &convertInfo));

	return dstSize;
}

void RHICooperativeVector::ConvertMatrix(const rhi::ConvertCooperativeMatrixDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(definition.srcData.size() > 0);

	SizeType dstSize = definition.dstData.size();

	VkConvertCooperativeVectorMatrixInfoNV convertInfo{ VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV };
	convertInfo.srcData.hostAddress = definition.srcData.data();
	convertInfo.dstData.hostAddress = definition.dstData.data();
	convertInfo.srcSize             = definition.srcData.size();
	convertInfo.pDstSize            = &dstSize;
	convertInfo.srcComponentType    = RHIToVulkan::GetCoomponentType(definition.srcType);
	convertInfo.dstComponentType    = RHIToVulkan::GetCoomponentType(definition.dstType);
	convertInfo.srcLayout           = RHIToVulkan::GetCooperativeVectorLayout(definition.srcLayout);
	convertInfo.dstLayout           = RHIToVulkan::GetCooperativeVectorLayout(definition.dstLayout);
	convertInfo.numRows             = definition.numRows;
	convertInfo.numColumns          = definition.numColumns;
	convertInfo.srcStride           = definition.srcStride;
	convertInfo.dstStride           = definition.dstStride;

	SPT_VK_CHECK(vkConvertCooperativeVectorMatrixNV(VulkanRHI::GetDeviceHandle(), &convertInfo));
}

} // spt::vulkan
