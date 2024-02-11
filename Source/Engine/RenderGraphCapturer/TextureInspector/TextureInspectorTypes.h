#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rg::capture
{

BEGIN_SHADER_STRUCT(TextureInspectorFilterParams)
	SHADER_STRUCT_FIELD(math::Vector2i, hoveredPixel)
	SHADER_STRUCT_FIELD(Real32, minValue)
	SHADER_STRUCT_FIELD(Real32, maxValue)
	SHADER_STRUCT_FIELD(Uint32, rChannelVisible)
	SHADER_STRUCT_FIELD(Uint32, gChannelVisible)
	SHADER_STRUCT_FIELD(Uint32, bChannelVisible)
	SHADER_STRUCT_FIELD(Uint32, isIntTexture)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(TextureInspectorReadbackData)
	SHADER_STRUCT_FIELD(math::Vector4f, hoveredPixelValue)
END_SHADER_STRUCT();


class TextureInspectorReadback
{
public:

	TextureInspectorReadback() = default;
	
	TextureInspectorReadbackData GetData() const
	{
		lib::LockGuard lock(m_readbackDataLock);
		const TextureInspectorReadbackData result = m_readbackData;
		return result;
	}

	void SetData(const TextureInspectorReadbackData& data)
	{
		lib::LockGuard lock(m_readbackDataLock);
		m_readbackData = data;
	}

private:

	TextureInspectorReadbackData m_readbackData;
	mutable lib::Lock            m_readbackDataLock;
};


} // spt::rg::capture