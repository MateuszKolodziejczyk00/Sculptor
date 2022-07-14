
project "SculptorLub"
	kind "SharedLib"
	language "C++"

	targetdir ("Binaries/" .. OutputDirectory .. "/%{prj.name}")
	objdir ("Intermediate/" .. OutputDirectory .. "/%{prj.name}")