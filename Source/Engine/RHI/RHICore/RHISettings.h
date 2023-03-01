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
	Bool IsRayTracingEnabled() const { return enableStrictValidation; }
	Bool AreGPUCrashDumpsEnabled() const { return enableGPUCrashDumps; }

private:

	static inline const lib::HashedString enableValidationCmdArgName = "-EnableValidation";
	Bool enableValidation = false;
	
	static inline const lib::HashedString enableStrictValidationCmdArgName = "-EnableStrictValidation";
	Bool enableStrictValidation = false;
	
	static inline const lib::HashedString enableRayTracingCmdArgName = "-EnableRayTracing";
	Bool enableRayTracing = false;

	static inline const lib::HashedString enableGPUCrashDumpsCmdArgName = "-EnableGPUCrashDumps";
	Bool enableGPUCrashDumps = false;
};

} // spt::rhi