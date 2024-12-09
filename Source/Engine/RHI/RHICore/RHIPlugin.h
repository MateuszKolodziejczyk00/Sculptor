#pragma once

#include "SculptorCoreTypes.h"
#include "RHIMacros.h"
#include "Utility/Templates/Overload.h"

#include <variant>


namespace spt::rhi
{

struct Extension
{
public:

	Extension() = default;

	Extension(lib::StringView name)
		: m_storage(name)
	{
	}
	
	Extension(lib::String name)
		: m_storage(name)
	{
	}

	const char* GetCStr() const
	{
		return lib::CallOverload(m_storage,
								 [](const lib::String& str)     -> const char* { return str.c_str(); },
								 [](const lib::StringView& str) -> const char* { return str.data(); });
	}

	lib::StringView GetStringView() const
	{
		return lib::CallOverload(m_storage,
								 [](const lib::String& str)     -> lib::StringView { return str; },
								 [](const lib::StringView& str) -> lib::StringView { return str; });
	}

private:

	std::variant<lib::String, lib::StringView> m_storage;
};


class ExtensionsCollector
{
public:
	
	ExtensionsCollector(lib::DynamicArray<Extension>& rhiExtensions, lib::DynamicArray<Extension>& deviceExtensions)
		: m_rhiExtensions(rhiExtensions)
		, m_deviceExtensions(deviceExtensions)
	{
	}

	void AddRHIExtension(Extension extension)    { m_rhiExtensions.emplace_back(extension); }
	void AddDeviceExtension(Extension extension) { m_deviceExtensions.emplace_back(extension); }
	 
private:

	lib::DynamicArray<Extension>& m_rhiExtensions;
	lib::DynamicArray<Extension>& m_deviceExtensions;
};


class RHI_API RHIPlugin
{
public:

	RHIPlugin();
	virtual ~RHIPlugin() = default;

	virtual void CollectExtensions(ExtensionsCollector& collector) const {};
};


class RHIPluginsManager
{
public:

	static RHIPluginsManager& Get();

	static void RegisterPlugin(RHIPlugin& provider);

	void CollectExtensions();

	lib::Span<const Extension> GetRHIExtensions() const    { return m_rhiExtensions; }
	lib::Span<const Extension> GetDeviceExtensions() const { return m_deviceExtensions; }

private:
	
	RHIPluginsManager() = default;

	lib::DynamicArray<RHIPlugin*> m_plugins;

	lib::DynamicArray<Extension> m_rhiExtensions;
	lib::DynamicArray<Extension> m_deviceExtensions;
};

} // spt::rhi
