
project "SculptorEd"
	kind "ConsoleApp"
	language "C++"

	targetdir ("Binaries/" .. OutputDirectory .. "/%{prj.name}")
	objdir ("Intermediate/" .. OutputDirectory .. "/%{prj.name}")