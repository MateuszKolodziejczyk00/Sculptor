#include "Module.h"
#include "FileSystem/DirectoryWatcher.h"
#include "Paths.h"

#ifdef SPT_PLATFORM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#endif
#pragma optimize("", off)

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

	for (const LoadedModule& moduleHandle : m_modules)
	{
		delete[] moduleHandle.apiPtr;
		delete[] moduleHandle.abi.exports.data();

		utils::UnloadModule(moduleHandle.handle);
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

ModuleHandle ModulesManager::LoadModule(lib::StringView moduleName, const lib::ModuleABI& abi)
{
	const lib::LockGuard lock{m_modulesLock};

	const lib::Path modulePath = lib::Path(Paths::GetExecutableDirectory()) / (lib::String(moduleName) + ".dll");

	ModuleHandle handle = utils::LoadModuleCopy(modulePath);
	if (!handle.IsValid())
	{
		SPT_LOG_ERROR(ModulesManager, "Failed to load module {} from path {}", moduleName, modulePath.string());
		return ModuleHandle();
	}

	Uint32 apiSize = 0u;
	for (const lib::ModuleExportDescriptor& exportDesc : abi.exports)
	{
		const Uint32 endOffset = exportDesc.apiOffset + sizeof(void*);
		if (endOffset > apiSize)
		{
			apiSize = endOffset;
		}
	}

	// Allocate local copy for ABI. Previous one might be unloaded so it's not safe to use it, even if it's static
	lib::ModuleExportDescriptor* localExports = new lib::ModuleExportDescriptor[abi.exports.size()];
	std::memcpy(localExports, abi.exports.data(), sizeof(lib::ModuleExportDescriptor) * abi.exports.size());

	lib::ModuleABI localABI;
	localABI.apiType = abi.apiType;
	localABI.exports = { localExports, abi.exports.size() };

	LoadedModule loadedModule;
	loadedModule.handle = handle;
	loadedModule.apiPtr = new Byte[apiSize];
	loadedModule.abi    = localABI;
	loadedModule.path   = modulePath;

	SPT_CHECK(moduleName.size() < sizeof(loadedModule.name));
	std::memcpy(loadedModule.name, moduleName.data(), std::min(moduleName.size(), sizeof(loadedModule.name) - 1));
	loadedModule.name[std::min(moduleName.size(), sizeof(loadedModule.name) - 1)] = '\0';

	std::memset(loadedModule.apiPtr, 0, apiSize);

	LoadModuleAPI_Locked(loadedModule.apiPtr, abi, handle);

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

		delete[] moduleToUnload.apiPtr;
		delete[] moduleToUnload.abi.exports.data();

		utils::UnloadModule(moduleToUnload.handle);

		SPT_LOG_INFO(ModulesManager, "Module {} unloaded successfully", moduleToUnload.name);

		m_modules.RemoveAtSwap(foundModuleIdx);
	}

}

void ModulesManager::EnqueueModuleReload(lib::StringView moduleName)
{
	const lib::LockGuard lock{m_modulesLock};

	for (const LoadedModule& moduleHandle : m_modules)
	{
		if (lib::StringView(moduleHandle.name) == moduleName)
		{
			m_modulesToReload.AddUnique(moduleHandle.handle);
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

Bool ModulesManager::ReloadModule_Locked(LoadedModule& moduleHandle)
{
	ModuleHandle newModule = utils::LoadModuleCopy(moduleHandle.path);
	if (newModule.IsValid())
	{
		utils::UnloadModule(moduleHandle.handle);
		moduleHandle.handle = newModule;

		LoadModuleAPI_Locked(moduleHandle.apiPtr, moduleHandle.abi, moduleHandle.handle);

		SPT_LOG_INFO(ModulesManager, "Module {} reloaded successfully", moduleHandle.name);

		return true;
	}
	else
	{
		SPT_LOG_ERROR(ModulesManager, "Failed to reload module {} from path {}", moduleHandle.name, moduleHandle.path.string());

		return false;
	}
}

void ModulesManager::LoadModuleAPI_Locked(Byte* apiPtr, const lib::ModuleABI& abi, const ModuleHandle& handle)
{
	SPT_CHECK(apiPtr != nullptr);

	for (const lib::ModuleExportDescriptor& exportDesc : abi.exports)
	{
		void* funcPtr = handle.GetFunction<void*>(exportDesc.name);
		*reinterpret_cast<void**>(apiPtr + exportDesc.apiOffset) = funcPtr;
	}
}

} // spt::engn
