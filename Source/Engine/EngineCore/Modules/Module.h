#pragma once

#include "Containers/InlineArray.h"
#include "EngineCoreMacros.h"
#include "FileSystem/DirectoryWatcher.h"
#include "FileSystem/File.h"
#include "SculptorCoreTypes.h"
#include "Utility/ModulesABI.h"


namespace spt::engn
{

using RawModuleHandle = Uint64;
static constexpr RawModuleHandle g_invalidModuleHandle = 0u;


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

	ModuleHandle LoadModule(lib::StringView moduleName, const lib::ModuleABI& abi);
	void         UnloadModule(const ModuleHandle& module);

	void EnqueueModuleReload(lib::StringView moduleName);
	void EnqueueModuleReload(const ModuleHandle& module);
	void ProcessModuleReloads();
	Bool HasPendingReloads() const;

	template<typename TModuleAPI>
	TModuleAPI* GetModuleAPI(const ModuleHandle& handle) const
	{
		for (const LoadedModule& module : m_modules)
		{
			if (module.handle.GetRawHandle() == handle.GetRawHandle())
			{
				if (module.abi.apiType == lib::TypeInfo<TModuleAPI>())
				{
					return reinterpret_cast<TModuleAPI*>(module.apiPtr);
				}
			}
		}

		return nullptr;
	}

	template<typename TModuleAPI>
	TModuleAPI* GetModuleAPI() const
	{
		for (const LoadedModule& loadedModule : m_modules)
		{
			if (loadedModule.abi.apiType == lib::TypeInfo<TModuleAPI>())
			{
				return reinterpret_cast<TModuleAPI*>(loadedModule.apiPtr);
			}
		}

		return nullptr;
	}

private:

	struct LoadedModule
	{
		static constexpr SizeType s_maxPathLength = 60u;

		ModuleHandle   handle;
		char           name[s_maxPathLength] = {};
		Byte*          apiPtr = nullptr;
		lib::Path      path;
		lib::ModuleABI abi;
	};

	Bool ReloadModule_Locked(LoadedModule& module);

	void LoadModuleAPI_Locked(Byte* apiPtr, const lib::ModuleABI& abi, const ModuleHandle& handle);

	static constexpr SizeType s_maxLoadedModules = 32u;

	lib::InlineArray<LoadedModule, s_maxLoadedModules> m_modules;
	lib::InlineArray<ModuleHandle, s_maxLoadedModules> m_modulesToReload;

	lib::Lock m_modulesLock;

	lib::FileWatcherHandle m_fileWatcherHandle = {};
};

} // spt::engn
