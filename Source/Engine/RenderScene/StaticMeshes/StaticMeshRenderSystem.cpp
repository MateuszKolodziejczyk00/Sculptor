#include "StaticMeshRenderSystem.h"

namespace spt::rsc
{

StaticMeshRenderSystem::StaticMeshRenderSystem()
{
	m_supportedStages = lib::Flags(ERenderStage::BasePassStage, ERenderStage::ShadowGenerationStage);
}

} // spt::rsc
