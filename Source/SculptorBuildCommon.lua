-- Utility functions ==================================================================================

function AppendTable(t1, t2)
    for k, v in pairs(t2)
    do
        t1[k] = v
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
    SharedLibrary       = "SharedLib",
    StaticLibrary       = "StaticLib",
    None                = "None"
}

ELanguage = 
{
    cpp                 = "C++",
    C                   = "C"
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

projectToPublicDefines = {}

currentProjectsType = nil

function Project:CreateProject(name, targetType, language)
    local projectType = currentProjectsType
    local inProjectLocation = projectType .. "/" .. name

    if projectType == EProjectType.ThirdParty then
        inProjectLocation = inProjectLocation .. "/" .. name
    end

    newProject =
    {
        ["name"] = name,
        ["targetType"] = targetType,
        ["projectType"] = projectType,
        ["language"] = language or ELanguage.CPP,
        -- reference path from other projects to this project
        referenceProjectLocation = "../../" .. inProjectLocation,
        configurations =
        {
            [EConfiguration.Debug] = {
                publicDependencies = {},
                privateDependencies = {},
                publicDefines = {}
            },
            [EConfiguration.Development] = 
            {
                publicDependencies = {},
                privateDependencies = {},
                publicDefines = {}
            },
            [EConfiguration.Release] = 
            {
                publicDependencies = {},
                privateDependencies = {},
                publicDefines = {}
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
    language (self.language)
    staticruntime "off"

    if self.projectType == EProjectType.ThirdParty then
        location (self.name)
    end

	targetdir ("../../../Binaries/" .. OutputDirectory)
	objdir ("../../../Intermediate/" .. OutputDirectory)

    projectToIncludePaths[self.name] = self:GetIncludePathsToThisProject()

    if self.projectType == EProjectType.ThirdParty then
        includedirs (self.name .. "/" .. self:GetIncludesRelativeLocation())
        includedirs (self.name)
    else
        includedirs ("" .. self:GetIncludesRelativeLocation())
    end

    includedirs (self:GetPrivateIncludePaths())

	libdirs ("../../../Binaries/" .. OutputDirectory)

    libdirs (self:GetAdditionalLibPaths())

    projectToPublicDependencies[self.name] = {}
    projectToPublicDefines[self.name] = {}

    filter "configurations:Debug"
    self:BuildConfiguration(EConfiguration.Debug, EPlatform.Windows)

    filter "configurations:Development"
    self:BuildConfiguration(EConfiguration.Development, EPlatform.Windows)

    filter "configurations:Release"
    self:BuildConfiguration(EConfiguration.Release, EPlatform.Windows)

end

function Project:SetupConfiguration(configuration, platform)
    
end

function Project:GetIncludePathsToThisProject()
    return { self.referenceProjectLocation .. self:GetIncludesRelativeLocation() }
end

function Project:GetPrivateIncludePaths()
    return {}
end

function Project:GetIncludesRelativeLocation()
    return  ""
end

function Project:AddPublicIncludePath(paths)

end

function Project:AddPrivateIncludePath(path)
    
end

function Project:GetProjectFiles(configuration, platform)
    return
    {
		"**.h",
		"**.cpp",
		"**.inl",
        "**.c"
    }
end

function Project:GetAdditionalLibPaths()

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

    projectToPublicDefines[self.name][configuration] = self:GetThisProjectPublicDefines(configuration)
    for define, _ in pairs(projectToPublicDefines[self.name][configuration])
    do
        self:AddDefineInternal(define)
    end

    local privateDependenciesPublicDefines = self:GetPrivateDependenciesPublicDefines(configuration)
    for define, _ in pairs(privateDependenciesPublicDefines)
    do
        self:AddDefineInternal(define)
    end

	files (self:GetProjectFiles(configuration, platform))

    if self:AreWarningsDisabled(configuration) then
        disablewarnings { "warning" }
    end
end

function Project:AddPublicDefine(define)
    -- public defines are added later in BuildConfiguration
    self.configurations[self.currentConfiguration].publicDefines[define] = true
end

function Project:AddPrivateDefine(define)
    self:AddDefineInternal(define)
end

function Project:AddDefineInternal(define)
    defines { define }
end

function Project:AddPublicDependency(dependency)
    self.configurations[self.currentConfiguration].publicDependencies[dependency] = true
end

function Project:AddPrivateDependency(dependency)
    self.configurations[self.currentConfiguration].privateDependencies[dependency] = true
    self:AddDependencyInternal(dependency)
end

function Project:DisableWarnings()
    self.configurations[self.currentConfiguration].disableWarnings = true
end

function Project:AreWarningsDisabled(configuration)
    disable = self.configurations[configuration]["disableWarnings"]
    return disable and disable == true
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

function Project:GetThisProjectPublicDefines(configuration)
    local allPublicDefines = self.configurations[configuration].publicDefines
    for dependency, _ in pairs(self.configurations[configuration].publicDependencies)
    do
        AppendTable(allPublicDefines, projectToPublicDefines[dependency][configuration])
    end

    return allPublicDefines
end

function Project:GetPrivateDependenciesPublicDefines(configuration)
    local allPublicDefines = self.configurations[configuration].publicDefines
    for dependency, _ in pairs(self.configurations[configuration].privateDependencies)
    do
        local projectDefines = projectToPublicDefines[dependency]
        if projectDefines ~= nil then
            local definesForConfiguration = projectDefines[configuration]
            if definesForConfiguration ~= nil then
                AppendTable(allPublicDefines, definesForConfiguration)
            end
        end
    end

    return allPublicDefines
end

function Project:AddCommonDefines(configuration, platform)
    if self.targetType == ETargetType.SharedLibrary then
        self:AddDefineInternal(string.upper(self.name) .. "_BUILD_DLL")
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
    self:AddDefineInternal("SPT_PLATFORM_WINDOWS")
end

function Project:AddDebugDefines()
    self:AddDefineInternal("SPT_DEBUG")
end

function Project:AddDevelopmentDefines()
    self:AddDefineInternal("SPT_DEVELOPMENT")
end

function Project:AddReleaseDefines()
    self:AddDefineInternal("SPT_RELEASE")
end


function StartProjectsType(projectsType)
    currentProjectsType = projectsType
end


function IncludeProject(name)
    include (currentProjectsType .. "/" .. name .. "/" .. name)
end