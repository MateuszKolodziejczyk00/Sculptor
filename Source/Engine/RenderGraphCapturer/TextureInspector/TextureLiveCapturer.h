#pragma once

#include "SculptorCoreTypes.h"
#include "RenderGraphDebugDecorator.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rg::capture
{

class LiveTextureOutput
{
public:

	void SetTexture(lib::SharedPtr<rdr::TextureView> texture)
	{
		const lib::LockGuard lock(m_textureLock);
		m_texture = std::move(texture);
	
	}

	lib::SharedPtr<rdr::TextureView> GetTexture()
	{
		const lib::LockGuard lock(m_textureLock);
		return m_texture;
	}

private:

	lib::SharedPtr<rdr::TextureView> m_texture;
	lib::Lock m_textureLock;
};


struct LiveCaptureDestTexture
{
	lib::HashedString textureName;
	lib::HashedString passName;

	lib::SharedPtr<LiveTextureOutput> output;
};


class TextureLiveCapturer : public RenderGraphDebugDecorator
{
public:

	explicit TextureLiveCapturer(const LiveCaptureDestTexture& destTexture);

	// Begin RenderGraphDebugDecorator interface
	virtual void PostNodeAdded(RenderGraphBuilder& graphBuilder, RGNode& node, const RGDependeciesContainer& dependencies) override;
	// End RenderGraphDebugDecorator interface

private:

	LiveCaptureDestTexture m_destTexture;
};

} // spt::rg::capture