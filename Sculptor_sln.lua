workspace "Sculptor"
    
	configurations
	{
		"Debug",
		"Development",
		"Release"
	}
    
	platforms
	{
		"x86"
	}

	OutputDirectory = "%{cfg.buildcfg}_%{cfg.system}_%{cfg.architecture}"
	
	exceptionhandling "Off"
    rtti "Off"

	cppdialect "C++17"
	
	flags { "FatalWarnings", "MultiProcessorCompile" }
	
	filter { "platforms:x64"}
        architecture "x86_64"
	
	filter {"configurations:Debug"}
        symbols "On"
		optimize "Off"
        
		defines
		{
			"SPT_DEBUG"
		}
		
	filter {"configurations:Development"}
        symbols "On"
		optimize "On"
        
		defines
		{
			"SPT_DEVELOPMENT"
		}
		
	filter {"configurations:Release"}
        symbols "Off"
		optimize "On"
        
		defines
		{
			"SPT_RELEASE"
		}
		
	group "ThirdParty"
		include "Source/SculptorThirdParty"
	
	group "Engine"
		include "Source/SculptorEngine"
	
	group "Editor"
		include "Source/SculptorEditor"
	