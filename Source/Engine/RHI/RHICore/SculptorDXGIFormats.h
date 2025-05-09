#pragma once

#include "SculptorCoreTypes.h"
#include "RHITextureTypes.h"

#include "dxgiformat.h"

namespace spt::rhi
{

rhi::EFragmentFormat DXGI2RHIFormat(DXGI_FORMAT dxgiFormat)
{
	switch (dxgiFormat)
	{
	case DXGI_FORMAT_UNKNOWN:            return EFragmentFormat::None;
	case DXGI_FORMAT_R8_UNORM:           return EFragmentFormat::R8_UN_Float;
	case DXGI_FORMAT_R8_UINT:            return EFragmentFormat::R8_U_Int;
	case DXGI_FORMAT_R16_UINT:           return EFragmentFormat::R16_U_Int;
	case DXGI_FORMAT_R16_UNORM:          return EFragmentFormat::R16_UN_Float;
	case DXGI_FORMAT_R16_FLOAT:          return EFragmentFormat::R16_S_Float;
	case DXGI_FORMAT_R32_FLOAT:          return EFragmentFormat::R32_S_Float;
	case DXGI_FORMAT_R32_UINT:           return EFragmentFormat::R32_U_Int;
	case DXGI_FORMAT_R8G8_UNORM:         return EFragmentFormat::RG8_UN_Float;
	case DXGI_FORMAT_R16G16_UINT:        return EFragmentFormat::RG16_U_Int;
	case DXGI_FORMAT_R16G16_UNORM:       return EFragmentFormat::RG16_UN_Float;
	case DXGI_FORMAT_R16G16_SNORM:       return EFragmentFormat::RG16_SN_Float;
	case DXGI_FORMAT_R16G16_FLOAT:       return EFragmentFormat::RG16_S_Float;
	case DXGI_FORMAT_R32G32_FLOAT:       return EFragmentFormat::RG32_S_Float;
	case DXGI_FORMAT_R32G32B32_FLOAT:    return EFragmentFormat::RGB32_S_Float;
	case DXGI_FORMAT_R10G10B10A2_UNORM:  return EFragmentFormat::RGB10A2_UN_Float;
	case DXGI_FORMAT_R11G11B10_FLOAT:    return EFragmentFormat::B10G11R11_U_Float;
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP: return EFragmentFormat::RGB9E5_Float;
	case DXGI_FORMAT_R8G8B8A8_UNORM:     return EFragmentFormat::RGBA8_UN_Float;
	case DXGI_FORMAT_B8G8R8A8_UNORM:     return EFragmentFormat::BGRA8_UN_Float;
	case DXGI_FORMAT_R16G16B16A16_UNORM: return EFragmentFormat::RGBA16_UN_Float;
	case DXGI_FORMAT_R16G16B16A16_FLOAT: return EFragmentFormat::RGBA16_S_Float;
	case DXGI_FORMAT_R32G32B32A32_FLOAT: return EFragmentFormat::RGBA32_S_Float;
	case DXGI_FORMAT_D16_UNORM:          return EFragmentFormat::D16_UN_Float;
	case DXGI_FORMAT_D32_FLOAT:          return EFragmentFormat::D32_S_Float;
	default:
		SPT_CHECK_NO_ENTRY_MSG("Unsupported DXGI Format"); return EFragmentFormat::None;
	}

}
DXGI_FORMAT RHI2DXGIFormat(rhi::EFragmentFormat rhiFormat)
{
	switch (rhiFormat)
	{
	case EFragmentFormat::None:                 return DXGI_FORMAT_UNKNOWN;
	case EFragmentFormat::R8_UN_Float:          return DXGI_FORMAT_R8_UNORM;
	case EFragmentFormat::R8_U_Int:             return DXGI_FORMAT_R8_UINT;
	case EFragmentFormat::R16_U_Int:            return DXGI_FORMAT_R16_UINT;
	case EFragmentFormat::R16_UN_Float:         return DXGI_FORMAT_R16_UNORM;
	case EFragmentFormat::R16_S_Float:          return DXGI_FORMAT_R16_FLOAT;
	case EFragmentFormat::R32_S_Float:          return DXGI_FORMAT_R32_FLOAT;
	case EFragmentFormat::R32_U_Int:            return DXGI_FORMAT_R32_UINT;
	case EFragmentFormat::RG8_UN_Float:         return DXGI_FORMAT_R8G8_UNORM;
	case EFragmentFormat::RG16_U_Int:           return DXGI_FORMAT_R16G16_UINT;
	case EFragmentFormat::RG16_UN_Float:        return DXGI_FORMAT_R16G16_UNORM;
	case EFragmentFormat::RG16_SN_Float:        return DXGI_FORMAT_R16G16_SNORM;
	case EFragmentFormat::RG16_S_Float:         return DXGI_FORMAT_R16G16_FLOAT;
	case EFragmentFormat::RG32_S_Float:         return DXGI_FORMAT_R32G32_FLOAT;
	case EFragmentFormat::RGB8_UN_Float:        SPT_CHECK_NO_ENTRY_MSG("Invalid DXGI Format"); return DXGI_FORMAT_UNKNOWN;
	case EFragmentFormat::BGR8_UN_Float:        SPT_CHECK_NO_ENTRY_MSG("Invalid DXGI Format"); return DXGI_FORMAT_UNKNOWN;
	case EFragmentFormat::RGB16_UN_Float:       SPT_CHECK_NO_ENTRY_MSG("Invalid DXGI Format"); return DXGI_FORMAT_UNKNOWN;
	case EFragmentFormat::RGB32_S_Float:        return DXGI_FORMAT_R32G32B32_FLOAT;
	case EFragmentFormat::RGB10A2_UN_Float:     return DXGI_FORMAT_R10G10B10A2_UNORM;
	case EFragmentFormat::B10G11R11_U_Float:    return DXGI_FORMAT_R11G11B10_FLOAT;
	case EFragmentFormat::A2B10G10R10_UN_Float: return DXGI_FORMAT_R10G10B10A2_UNORM;
	case EFragmentFormat::RGB9E5_Float:         return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
	case EFragmentFormat::RGBA8_UN_Float:       return DXGI_FORMAT_R8G8B8A8_UNORM;
	case EFragmentFormat::BGRA8_UN_Float:       return DXGI_FORMAT_B8G8R8A8_UNORM;
	case EFragmentFormat::RGBA16_UN_Float:      return DXGI_FORMAT_R16G16B16A16_UNORM;
	case EFragmentFormat::RGBA16_S_Float:       return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case EFragmentFormat::RGBA32_S_Float:       return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case EFragmentFormat::D16_UN_Float:         return DXGI_FORMAT_D16_UNORM;
	case EFragmentFormat::D32_S_Float:          return DXGI_FORMAT_D32_FLOAT;
	default:
		SPT_CHECK_NO_ENTRY_MSG("Invalid DXGI Format"); return DXGI_FORMAT_UNKNOWN;
	}
}

} // spt::rhi