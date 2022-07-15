-- Utility functions ==================================================================================

function AppendTable(t1, t2)
    for k, v in pairs(t2)
    do
        t1.insert(k, v)
    end
end

-- Project Generation =================================================================================

EConfiguration =
{
    Debug               = 1,
    Development         = 2,
    Release             = 3
}

EPlatform =
{
    Windows             = 1
}

ETargetType = 
{
    Application         = "ConsoleApp",
    SharedLibrary       = "SharedLib"
}

EProjectType =
{
    Engine              = "Engine",
    Editor              = "Editor",
    ThirdParty          = "ThirdParty"
}

Project =
{
}

OutputDirectory = "%{cfg.buildcfg}_%{cfg.system}_%{cfg.architecture}"

projectToIncludePaths = {}

projectToPublicDependencies = {}


function Project:CreateProject(name, targetType, projectType)
    inProjectLocation = "Source/" .. projectType .. "/" .. name

    newProject =
    {
        ["name"] = name,
        ["targetType"] = targetType,
        ["projectType"] = projectType,
        projectLocation = inProjectLocation,
        configurations =
        {
            [EConfiguration.Debug] = {
                publicDependencies = {},
                privateDependencies = {}
            },
            [EConfiguration.Development] = 
            {
                publicDependencies = {},
                privateDependencies = {}
            },
            [EConfiguration.Release] = 
            {
                publicDependencies = {},
                privateDependencies = {}
            }
        }
    }

    setmetatable(newProject, self)
    self.__index = self
    return newProject
end

function Project:SetupProject()
    project (self.name)
    kind (self.targetType)
    language "C++"
    staticruntime "off"

	targetdir ("Binaries/" .. OutputDirectory .. "/%{prj.name}")
	objdir ("Intermediate/" .. OutputDirectory .. "/%{prj.name}")

    projectToIncludePaths[self.name] = self:GetIncludePaths()

	files
	{
		"**.h",
		"**.cpp",
		"**.inl"
	}

    projectToPublicDependencies[self.name] = {}

    filter "configurations:Debug"
    self:BuildConfiguration(EConfiguration.Debug, EPlatform.Windows)

    filter "configurations:Development"
    self:BuildConfiguration(EConfiguration.Development, EPlatform.Windows)

    filter "configurations:Release"
    self:BuildConfiguration(EConfiguration.Release, EPlatform.Windows)

end

function Project:SetupConfiguration(configuration, platform)
    
end

function Project:GetIncludePaths()
    return { self.projectLocation }
end

function Project:BuildConfiguration(configuration, platform)
    self.currentConfiguration = configuration
    self:SetupConfiguration(configuration, platform)
    projectToPublicDependencies[self.name][configuration] = self:GetPublicDependencies(configuration)

    for dependency, _ in pairs(projectToPublicDependencies[self.name][configuration])
    do
        self:AddDependencyInternal(dependency)
    end

    for dependency, _ in pairs(self.configurations[configuration].privateDependencies)
    do
        self:AddDependencyInternal(dependency)
    end

    self:AddCommonDefines(configuration, platform)
end

function Project:AddDefine(define)
    defines { define }
end

function Project:AddPublicDependency(dependency)
    self.configurations[self.currentConfiguration].publicDependencies[dependency] = true
end

function Project:AddPrivateDependency(dependency)
    self.configurations[self.currentConfiguration].privateDependencies[dependency] = true
    self:AddDependencyInternal(dependency)
end

function Project:AddDependencyInternal(dependency)
    links { dependency }
    includedirs (projectToIncludePaths[dependency])
end

function Project:GetPublicDependencies(configuration)
    local allPublicDependencies = self.configurations[configuration].publicDependencies
    for dependency, _ in pairs(self.configurations[configuration].publicDependencies)
    do
        AppendTable(allPublicDependencies, projectToPublicDependencies[dependency][configuration])
    end

    return allPublicDependencies
end

function Project:AddCommonDefines(configuration, platform)
    if self.targetType == ETargetType.SharedLibrary then
        self:AddDefine(string.upper(self.name) .. "BUILD_DLL")
    end

    if configuration == EConfiguration.Debug then
        self:AddDebugDefines()
    elseif configuration == EConfiguration.Development then
        self:AddWindowsDefines()
    elseif configuration == EConfiguration.Release then
        self:AddReleaseDefines()
    end

    if platform == EPlatform.Windows then
        self:AddWindowsDefines()
    end
end

function Project:AddWindowsDefines()
    self:AddDefine("SPT_PLATFORM_WINDOWS")
end

function Project:AddDebugDefines()
    self:AddDefine("SPT_DEBUG")
end

function Project:AddDevelopmentDefines()
    self:AddDefine("SPT_DEVELOPMENT")
end

function Project:AddReleaseDefines()
    self:AddDefine("SPT_RELEASE")
end