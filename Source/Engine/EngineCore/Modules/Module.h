#pragma once

#include "Containers/InlineArray.h"
#include "EngineCoreMacros.h"
#include "FileSystem/DirectoryWatcher.h"
#include "FileSystem/File.h"
#include "SculptorCoreTypes.h"
#include "Utility/ModuleDescriptor.h"


namespace spt::rdr
{
struct GPUApiData;
} // spt::rdr


namespace spt::engn
{

class Engine;


using RawModuleHandle = Uint64;
static constexpr RawModuleHandle g_invalidModuleHandle = 0u;


struct EngineGlobals
{
	prf::ProfilerImpl*       profiler           = nullptr;
	Engine*                  engineInstance     = nullptr;
	rdr::GPUApiData*         gpuApiData         = nullptr;
	lib::HashedStringDBData* hashedStringDBData = nullptr;
};


class ModuleHandle
{
public:

	ModuleHandle() = default;
	explicit ModuleHandle(RawModuleHandle handle)
		: m_handle(handle)
	{}

	bool IsValid() const { return m_handle != g_invalidModuleHandle; }

	template<typename T>
	T* GetFunction(const char* funcName) const
	{
		return reinterpret_cast<T*>(GetFunctionImpl(funcName));
	}

	RawModuleHandle GetRawHandle() const { return m_handle; }

	Bool operator==(const ModuleHandle& other) const { return m_handle == other.m_handle; }
	Bool operator!=(const ModuleHandle& other) const { return m_handle != other.m_handle; }

private:

	void* GetFunctionImpl(const char* funcName) const;

	RawModuleHandle m_handle = g_invalidModuleHandle;
};


class ENGINE_CORE_API ModulesManager
{
public:

	ModulesManager();
	~ModulesManager();

	void Initialize();

	void InitialzeGlobals(EngineGlobals& globals);

	ModuleHandle LoadModule(lib::StringView moduleName);
	void         UnloadModule(const ModuleHandle& module);

	void EnqueueModuleReload(lib::StringView moduleName);
	void EnqueueModuleReload(const ModuleHandle& module);
	void ProcessModuleReloads();
	Bool HasPendingReloads() const;

	template<typename TModuleAPI>
	const TModuleAPI* GetModuleAPI(const ModuleHandle& handle) const
	{
		for (const LoadedModule& module : m_modules)
		{
			if (module.handle.GetRawHandle() == handle.GetRawHandle())
			{
				if (module.descriptor.apiType == lib::TypeInfo<TModuleAPI>())
				{
					return reinterpret_cast<const TModuleAPI*>(module.descriptor.api);
				}
				break;
			}
		}

		return nullptr;
	}

	template<typename TModuleAPI>
	const TModuleAPI* GetModuleAPI() const
	{
		for (const LoadedModule& loadedModule : m_modules)
		{
			if (loadedModule.descriptor.apiType == lib::TypeInfo<TModuleAPI>())
			{
				return reinterpret_cast<const TModuleAPI*>(loadedModule.descriptor.api);
			}
		}

		return nullptr;
	}

private:

	struct LoadedModule
	{
		static constexpr SizeType s_maxPathLength = 60u;

		ModuleHandle          handle;
		char                  name[s_maxPathLength] = {};
		lib::Path             path;
		lib::ModuleDescriptor descriptor;
	};

	Bool ReloadModule_Locked(LoadedModule& module);

	static constexpr SizeType s_maxLoadedModules = 32u;

	lib::InlineArray<LoadedModule, s_maxLoadedModules> m_modules;
	lib::InlineArray<ModuleHandle, s_maxLoadedModules> m_modulesToReload;

	EngineGlobals m_globals;

	lib::Lock m_modulesLock;

	lib::FileWatcherHandle m_fileWatcherHandle = {};
};

} // spt::engn
