#include "Module.h"
#include "FileSystem/DirectoryWatcher.h"
#include "Paths.h"
#include "Utility/Templates/Callable.h"

#ifdef SPT_PLATFORM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#endif


SPT_DEFINE_LOG_CATEGORY(ModulesManager, true);

namespace spt::engn
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// utils =========================================================================================

namespace utils
{

#ifdef SPT_PLATFORM_WINDOWS

RawModuleHandle LoadModuleImpl(const wchar_t* modulePath)
{
	return reinterpret_cast<RawModuleHandle>(::LoadLibraryW(modulePath));
}

void UnloadModuleImpl(RawModuleHandle moduleHandle)
{
	::FreeLibrary(reinterpret_cast<HMODULE>(moduleHandle));
}

void* GetFunctionImpl(RawModuleHandle moduleHandle, const char* funcName)
{
	return reinterpret_cast<void*>(::GetProcAddress(reinterpret_cast<HMODULE>(moduleHandle), funcName));
}

bool IsFileReady(const lib::Path& path)
{
#ifdef SPT_PLATFORM_WINDOWS
	const HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	::CloseHandle(hFile);
	return true;
#else
	std::error_code ec;
	return std::filesystem::exists(path, ec) && !ec;
#endif
}
#endif // SPT_PLATFORM_WINDOWS

ModuleHandle LoadModule(const lib::Path& modulePath)
{
	const RawModuleHandle handle = utils::LoadModuleImpl(modulePath.c_str());

	return ModuleHandle(handle);
}

void UnloadModule(const ModuleHandle &moduleHandle)
{
	if (moduleHandle.IsValid())
	{
		utils::UnloadModuleImpl(moduleHandle.GetRawHandle());
	}
}

ModuleHandle LoadModuleCopy(const lib::Path& modulePath)
{
	const lib::Path tempPath = Paths::Combine(Paths::GetExecutableDirectory(), "TempBin");

	std::error_code ec;
	std::filesystem::create_directories(tempPath, ec);

	const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	const lib::WString uniqueStem = modulePath.stem().wstring() + L"_" + std::to_wstring(stamp);

	const lib::Path copiedDllPath = tempPath / (uniqueStem + modulePath.extension().wstring());
	const lib::Path sourcePdbPath = modulePath.parent_path() / (modulePath.stem().wstring() + L".pdb");
	const lib::Path copiedPdbPath = tempPath / (uniqueStem + L".pdb");

	std::filesystem::copy_file(modulePath, copiedDllPath, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec)
	{
		SPT_LOG_ERROR(ModulesManager, "Failed to copy module from {} to {}. Error: {}", modulePath.string(), copiedDllPath.string(), ec.message());
		return ModuleHandle();
	}

	ec.clear();
	if (std::filesystem::exists(sourcePdbPath, ec) && !ec)
	{
		ec.clear();
		std::filesystem::copy_file(sourcePdbPath, copiedPdbPath, std::filesystem::copy_options::overwrite_existing, ec);
		if (ec)
		{
			SPT_LOG_WARN(ModulesManager, "Failed to copy pdb from {} to {}. Error: {}", sourcePdbPath.string(), copiedPdbPath.string(), ec.message());
		}
	}

	return LoadModule(copiedDllPath);
}

void InitializeModule(const ModuleHandle& moduleHandle, const EngineGlobals& globals, lib::ModuleDescriptor& outModuleDescriptor)
{
	constexpr const char* initializeFuncName = "InitializeModule";
	using InitializeModuleFunc = lib::RawCallable<void(const EngineGlobals*, lib::ModuleDescriptor&)>;

	const InitializeModuleFunc initializeFunc = moduleHandle.GetFunction<void(const EngineGlobals*, lib::ModuleDescriptor&)>(initializeFuncName);
	SPT_CHECK_MSG(initializeFunc.IsValid(), "Module does not export {} function", initializeFuncName);

	initializeFunc(&globals, outModuleDescriptor);
}

} // utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// ModuleHandle ==================================================================================

void* ModuleHandle::GetFunctionImpl(const char* funcName) const
{
	return IsValid() ? utils::GetFunctionImpl(GetRawHandle(), funcName) : nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ModuleManager =================================================================================

ModulesManager::ModulesManager()
{
}

ModulesManager::~ModulesManager()
{
	lib::StopWatchingDirectory(m_fileWatcherHandle);

	for (const LoadedModule& loadedModule : m_modules)
	{
		utils::UnloadModule(loadedModule.handle);
	}
}

void ModulesManager::Initialize()
{
	auto onFileModified = [this](const lib::FileModifiedPayload& payload)
	{
		if (payload.fileName.find(".dll") != lib::String::npos)
		{
			const lib::String fileNameWithoutExtension = lib::Path(payload.fileName).stem().string();
			EnqueueModuleReload(fileNameWithoutExtension);
		}
	};

	m_fileWatcherHandle = lib::StartWatchingDirectory(lib::Path(Paths::GetExecutableDirectory()), lib::FileModifiedCallback::CreateLambda(std::move(onFileModified)));
}

void ModulesManager::InitialzeGlobals(EngineGlobals& globals)
{
	m_globals = globals;
}

ModuleHandle ModulesManager::LoadModule(lib::StringView moduleName)
{
	const lib::LockGuard lock{m_modulesLock};

	const lib::Path modulePath = lib::Path(Paths::GetExecutableDirectory()) / (lib::String(moduleName) + ".dll");

	ModuleHandle handle = utils::LoadModuleCopy(modulePath);
	if (!handle.IsValid())
	{
		SPT_LOG_ERROR(ModulesManager, "Failed to load module {} from path {}", moduleName, modulePath.string());
		return ModuleHandle();
	}

	LoadedModule loadedModule;
	loadedModule.handle = handle;
	loadedModule.path   = modulePath;

	SPT_CHECK(moduleName.size() < sizeof(loadedModule.name));
	std::memcpy(loadedModule.name, moduleName.data(), std::min(moduleName.size(), sizeof(loadedModule.name) - 1));
	loadedModule.name[std::min(moduleName.size(), sizeof(loadedModule.name) - 1)] = '\0';

	utils::InitializeModule(handle, m_globals, loadedModule.descriptor);

	m_modules.PushBack(std::move(loadedModule));

	SPT_LOG_INFO(ModulesManager, "Module {} loaded successfully from path {}", moduleName, modulePath.string());

	return loadedModule.handle;
}

void ModulesManager::UnloadModule(const ModuleHandle& moduleHandle)
{
	const lib::LockGuard lock{m_modulesLock};

	Uint32 foundModuleIdx = idxNone<Uint32>;
	for (Uint32 moduleIdx = 0u; moduleIdx < m_modules.GetSize(); ++moduleIdx)
	{
		if (m_modules[moduleIdx].handle.GetRawHandle() == moduleHandle.GetRawHandle())
		{
			foundModuleIdx = moduleIdx;
			break;
		}
	}

	if (foundModuleIdx != idxNone<Uint32>)
	{
		const LoadedModule& moduleToUnload = m_modules[foundModuleIdx];

		utils::UnloadModule(moduleToUnload.handle);

		SPT_LOG_INFO(ModulesManager, "Module {} unloaded successfully", moduleToUnload.name);

		m_modules.RemoveAtSwap(foundModuleIdx);
	}

}

void ModulesManager::EnqueueModuleReload(lib::StringView moduleName)
{
	const lib::LockGuard lock{m_modulesLock};

	for (const LoadedModule& loadedModule : m_modules)
	{
		if (lib::StringView(loadedModule.name) == moduleName)
		{
			m_modulesToReload.AddUnique(loadedModule.handle);
			break;
		}
	}
}

void ModulesManager::EnqueueModuleReload(const ModuleHandle& moduleHandle)
{
	const lib::LockGuard lock{m_modulesLock};

	m_modulesToReload.AddUnique(moduleHandle);
}

void ModulesManager::ProcessModuleReloads()
{
	const lib::LockGuard lock{m_modulesLock};

	lib::InlineArray<ModuleHandle, ModulesManager::s_maxLoadedModules> failedReloads;

	for (const ModuleHandle& moduleHandle : m_modulesToReload)
	{
		Uint32 foundModuleIdx = idxNone<Uint32>;
		for (Uint32 moduleIdx = 0u; moduleIdx < m_modules.GetSize(); ++moduleIdx)
		{
			if (m_modules[moduleIdx].handle.GetRawHandle() == moduleHandle.GetRawHandle())
			{
				foundModuleIdx = moduleIdx;
				break;
			}
		}

		if (foundModuleIdx != idxNone<Uint32>)
		{
			if (!utils::IsFileReady(m_modules[foundModuleIdx].path))
			{
				failedReloads.PushBack(moduleHandle);
				continue;
			}

			if (!ReloadModule_Locked(m_modules[foundModuleIdx]))
			{
				failedReloads.PushBack(moduleHandle);
			}
		}
	}

	m_modulesToReload = std::move(failedReloads);
}

Bool ModulesManager::HasPendingReloads() const
{
	return !m_modulesToReload.IsEmpty();
}

Bool ModulesManager::ReloadModule_Locked(LoadedModule& loadedModule)
{
	ModuleHandle newModule = utils::LoadModuleCopy(loadedModule.path);
	if (newModule.IsValid())
	{
		utils::UnloadModule(loadedModule.handle);
		loadedModule.handle = newModule;

		utils::InitializeModule(loadedModule.handle, m_globals, loadedModule.descriptor);

		SPT_LOG_INFO(ModulesManager, "Module {} reloaded successfully", loadedModule.name);

		return true;
	}
	else
	{
		SPT_LOG_ERROR(ModulesManager, "Failed to reload module {} from path {}", loadedModule.name, loadedModule.path.string());

		return false;
	}
}

} // spt::engn
