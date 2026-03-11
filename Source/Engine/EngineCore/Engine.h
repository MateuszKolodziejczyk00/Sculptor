#pragma once

#include "EngineCoreMacros.h"
#include "Paths.h"
#include "Plugins/PluginsManager.h"
#include "SculptorCoreTypes.h"
#include "Utils/CommandLineArguments.h"
#include "Delegates/MulticastDelegate.h"
#include "EngineTimer.h"
#include "Plugins/PluginsManager.h"
#include "AssetsSystem.h"
#include "Modules/Module.h"


namespace spt::engn
{

struct EngineInitializationParams
{
	EngineInitializationParams()
	{ }

	platf::CmdLineArgs additionalArgs;
};


class ENGINE_CORE_API Engine
{
public:

	static Engine& Get();
	static void Initialize(const EngineInitializationParams& initializationParams);
	static void InitializeModule(Engine& engineInstance);

	Real32 BeginFrame();

	using  OnBeginFrameDelegate = lib::MulticastDelegate<void()>;
	OnBeginFrameDelegate& GetOnBeginFrameDelegate();

	const CommandLineArguments& GetCmdLineArgs();

	const EngineTimer& GetEngineTimer() const;
	Real32 GetTime() const;

	as::AssetsSystem& GetAssetsSystem() { return m_assetsSystem; }

	ModulesManager& GetModulesManager() { return m_modulesManager; }

	PluginsManager& GetPluginsManager() { return m_pluginsManager; }

	const Paths& GetPaths() const { return m_paths; }

	template<typename TSingleton>
	void RegisterSingleton(TSingleton& singleton)
	{
		const lib::TypeID typeID = lib::TypeInfo<TSingleton>();
		for (const SingletonData& data : m_singletons)
		{
			if (data.typeId == typeID)
			{
				return;
			}
		}

		m_singletons.push_back({ typeID, &singleton });
	}

	template<typename TSingleton>
	TSingleton* GetSingleton() const
	{
		const lib::TypeID typeId = lib::TypeInfo<TSingleton>();
		for (const SingletonData& data : m_singletons)
		{
			if (data.typeId == typeId)
			{
				return reinterpret_cast<TSingleton*>(data.dataPtr);
			}
		}

		return nullptr;
	}

private:

	Engine() = default;

	EngineInitializationParams m_initParams;

	CommandLineArguments m_cmdLineArgs;

	PluginsManager m_pluginsManager;

	EngineTimer m_timer;
	OnBeginFrameDelegate m_onBeginFrameDelegate;

	as::AssetsSystem m_assetsSystem;

	ModulesManager m_modulesManager;

	Paths m_paths;

	struct SingletonData
	{
		lib::TypeID typeId;
		void* dataPtr = nullptr;
	};

	lib::DynamicArray<SingletonData> m_singletons;
};


ENGINE_CORE_API Engine&            GetEngine();
ENGINE_CORE_API const EngineTimer& GetEngineTimer();


template<typename TSingleton>
struct TEngineSingleton
{
	static TSingleton& Get()
	{
		TSingleton* ptr = instance.load();
		if (!ptr)
		{
			lib::LockGuard lock(instanceLock);
			ptr = instance.load();
			if (!ptr) //check again, as another thread could already create instance while we were waiting for lock
			{
				Engine& engine = Engine::Get();
				instance = engine.GetSingleton<TSingleton>();
				if (!instance)
				{
					instance = new TSingleton();
					engine.RegisterSingleton(*instance);
				}
				ptr = instance.load();
			}
		}

		return *ptr;
	}

	static inline std::atomic<TSingleton*> instance = nullptr;
	static inline lib::Spinlock instanceLock;
};

} // spt::engn
