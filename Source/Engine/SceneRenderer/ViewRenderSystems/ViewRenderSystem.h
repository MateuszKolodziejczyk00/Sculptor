#pragma once

#include "RenderSceneMacros.h"
#include "SceneRendererTypes.h"
#include "SculptorCoreTypes.h"
#include "Utility/Templates/Callable.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class RenderView;
class ViewRenderingSpec;
class RenderViewsCollector;


class ViewRenderSystem
{
public:

	ViewRenderSystem(RenderView& inRenderView);

	void Initialize(RenderView& inRenderView) {}
	void Deinitialize(RenderView& inRenderView) {}

	void CollectRenderViews(const RenderScene& renderScene, const RenderView& owningView, INOUT RenderViewsCollector& viewsCollector) {};

	void PrepareRenderView(ViewRenderingSpec& viewSpec) {};

	void BeginFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) {};

	RenderView& GetOwningView() const;

private:

	RenderView& m_owningView;
};


class ViewRenderSystemsAPI
{
public:

	static ViewRenderSystemsAPI& Get();

	ViewRenderSystem* CallConstructor(EViewRenderSystem type, lib::MemoryArena& arena, RenderView& owningView) const;
	void              CallDestructor(EViewRenderSystem type, ViewRenderSystem& system) const;
	void              CallInitialize(EViewRenderSystem type, ViewRenderSystem& system, RenderView& owningView) const;
	void              CallDeinitialize(EViewRenderSystem type, ViewRenderSystem& system, RenderView& owningView) const;
	void              CallCollectRenderViews(EViewRenderSystem type, ViewRenderSystem& system, const RenderScene& renderScene, const RenderView& owningView, INOUT RenderViewsCollector& viewsCollector) const;
	void              CallPrepareRenderView(EViewRenderSystem type, ViewRenderSystem& system, ViewRenderingSpec& viewSpec) const;
	void              CallBeginFrame(EViewRenderSystem type, ViewRenderSystem& system, rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) const;

	template<typename TViewRenderSystem>
	void RegisterSystem()
	{
		constexpr EViewRenderSystem type = TViewRenderSystem::type;
		const Uint32 systemTypeIdx = GetViewRenderSystemIdx(type);

		const auto constructor = [](lib::MemoryArena& arena, RenderView& owningView) -> ViewRenderSystem*
		{
			return arena.AllocateType<TViewRenderSystem>(owningView);
		};

		const auto destructor = [](ViewRenderSystem& system)
		{
			static_cast<TViewRenderSystem&>(system).~TViewRenderSystem();
		};

		const auto initialize = [](ViewRenderSystem& system, RenderView& owningView)
		{
			static_cast<TViewRenderSystem&>(system).Initialize(owningView);
		};

		const auto deinitialize = [](ViewRenderSystem& system, RenderView& owningView)
		{
			static_cast<TViewRenderSystem&>(system).Deinitialize(owningView);
		};

		const auto collectRenderViews = [](ViewRenderSystem& system, const RenderScene& renderScene, const RenderView& owningView, INOUT RenderViewsCollector& viewsCollector)
		{
			static_cast<TViewRenderSystem&>(system).CollectRenderViews(renderScene, owningView, viewsCollector);
		};

		const auto prepareRenderView = [](ViewRenderSystem& system, ViewRenderingSpec& viewSpec)
		{
			static_cast<TViewRenderSystem&>(system).PrepareRenderView(viewSpec);
		};

		const auto executeBeginFrame = [](ViewRenderSystem& system, rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
		{
			static_cast<TViewRenderSystem&>(system).BeginFrame(graphBuilder, renderScene, viewSpec);
		};

		m_systems[systemTypeIdx] = SystemEntry
		{
			.constructor = constructor,
			.destructor = destructor,
			.initialize = initialize,
			.deinitialize = deinitialize,
			.collectRenderViews = collectRenderViews,
			.prepareRenderView = prepareRenderView,
			.beginFrame = executeBeginFrame
		};
	}

private:

	struct SystemEntry
	{
		lib::RawCallable<ViewRenderSystem*(lib::MemoryArena& /* arena */, RenderView& /* owningView */)> constructor;
		lib::RawCallable<void(ViewRenderSystem& /* system */)> destructor;
		lib::RawCallable<void(ViewRenderSystem& /* system */, RenderView& /* owningView */)> initialize;
		lib::RawCallable<void(ViewRenderSystem& /* system */, RenderView& /* owningView */)> deinitialize;
		lib::RawCallable<void(ViewRenderSystem& /* system */, const RenderScene& /* renderScene */, const RenderView& /* owningView */, INOUT RenderViewsCollector& /* viewsCollector */)> collectRenderViews;
		lib::RawCallable<void(ViewRenderSystem& /* system */, ViewRenderingSpec& /* viewSpec */)> prepareRenderView;
		lib::RawCallable<void(ViewRenderSystem& /* system */, rg::RenderGraphBuilder& /* graphBuilder */, const RenderScene& /* renderScene */, ViewRenderingSpec& /* viewSpec */)> beginFrame;
	};

	const SystemEntry& GetSystemEntry(EViewRenderSystem type) const
	{
		const Uint32 systemTypeIdx = GetViewRenderSystemIdx(type);
		SPT_CHECK(systemTypeIdx < MaxViewRenderSystemsNum);
		return m_systems[systemTypeIdx];
	}

	lib::StaticArray<SystemEntry, MaxViewRenderSystemsNum> m_systems;
};


template<typename TSystemType>
struct ViewRenderSystemAPIRegistration
{
	ViewRenderSystemAPIRegistration()
	{
		ViewRenderSystemsAPI::Get().RegisterSystem<TSystemType>();
	}
};

#define SPT_REGISTER_VIEW_RENDER_SYSTEM(TSystemType) \
	SCENE_RENDERER_API spt::rsc::ViewRenderSystemAPIRegistration<TSystemType> reg##TSystemType##Instance;

} // spt::rsc
