#pragma once

#include "SculptorCore.h"
#include "AssetTypes.h"
#include "RHICore//RHITextureTypes.h"


namespace spt::as
{

struct CloudscapeAssetDefinition;


struct CompiledCloudscapeHeader
{
	struct TextureInfo
	{
		math::Vector2u       resolution = math::Vector2u::Zero();
		rhi::EFragmentFormat format     = rhi::EFragmentFormat::None;
		Uint32               dataOffset = 0u;
		Uint32               dataSize   = 0u;
	};

	TextureInfo weatherMap{};
};


struct CloudscapeCompilationResult
{
	lib::DynamicArray<Byte> blob;
};


namespace cloudscape_compiler
{

std::optional<CloudscapeCompilationResult> CompileCloudscape(const AssetInstance& asset, const CloudscapeAssetDefinition& definition);

} // cloudscape_compiler

} // spt::as
