#include "StaticMeshesRenderSystem.h"

namespace spt::rsc
{

StaticMeshesRenderSystem::StaticMeshesRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::BasePassStage, ERenderStage::ShadowGenerationStage);
}

} // spt::rsc
