#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

class RHISettings
{
public:

	RHISettings();

	void Initialize();

	Bool IsValidationEnabled() const { return enableValidation; }
	Bool IsStrictValidationEnabled() const { return enableStrictValidation; }
	Bool IsRayTracingEnabled() const { return enableRayTracing; }
	Bool AreGPUCrashDumpsEnabled() const { return enableGPUCrashDumps; }
	Bool ArePersistentDebugNamesEnabled() const { return enablePersistentDebugNames; }

private:

	static inline const char* enableValidationCmdArgName = "-EnableValidation";
	Bool enableValidation = false;
	
	static inline const char* enableStrictValidationCmdArgName = "-EnableStrictValidation";
	Bool enableStrictValidation = false;
	
	static inline const char* enableRayTracingCmdArgName = "-EnableRayTracing";
	Bool enableRayTracing = false;

	static inline const char* enableGPUCrashDumpsCmdArgName = "-EnableGPUCrashDumps";
	Bool enableGPUCrashDumps = false;

	/** 
	 * If it's set, rhi objects debug names won't be reset. This allows to see the names in the debugger (f.e. Renderdoc).
	 * @Warning: It will cause memory leaks
	 */
	static inline const char* enablePersistentDebugNamesCmdArgName = "-RHIPersistentNames";
	Bool enablePersistentDebugNames = false;
};

} // spt::rhi
