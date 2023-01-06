#pragma once

#include "Delegates/MulticastDelegate.h"
#include "SceneRendererTypes.h"

namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class RenderView;

class RenderStageEntries
{
public:

	using PreRenderStageDelegate	= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, const RenderView& /*view*/)>;
	using OnRenderStageDelegate		= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, const RenderView& /*view*/)>;
	using PostRenderStageDelegate	= lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/, const RenderScene& /*scene*/, const RenderView& /*view*/)>;

	RenderStageEntries();

	PreRenderStageDelegate&		GetPreRenderStageDelegate();
	OnRenderStageDelegate&		GetOnRenderStage();
	PostRenderStageDelegate&	GetPostRenderStage();

private:

	PreRenderStageDelegate  m_preRenderStage;
	OnRenderStageDelegate	m_onRenderStage;
	PostRenderStageDelegate	m_postRenderStage;
};


class RenderViewEntryPoints
{
public:

	RenderViewEntryPoints();

	void Initialize(const RenderView& renderView);

	const RenderView& GetRenderView() const;
	Bool SupportsStage(ERenderStage stage) const;

	const RenderStageEntries&	GetRenderStageEntries(ERenderStage stage) const;
	RenderStageEntries&			GetRenderStageEntries(ERenderStage stage);

private:

	lib::HashMap<SizeType, RenderStageEntries> m_stagesEntries;

	const RenderView* m_renderView;
};

} // spt::rsc