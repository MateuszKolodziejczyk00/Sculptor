#include "SceneRenderer.h"

namespace spt::rsc
{

SceneRenderer::SceneRenderer()
{ }

const BasePassRenderStage& SceneRenderer::GetBasePassStage() const
{
	return m_basePass;
}

} // spt::rsc
