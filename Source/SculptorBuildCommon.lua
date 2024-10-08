-- Utility functions ======================================================================================

function AppendTable(t1, t2)
    for k, v in pairs(t2)
    do
        t1[k] = v
    end
end

function GetTableLength(table)
    local length = 0
    for k, v in pairs(table)
    do
        length = length + 1
    end

    return length
end

function Copy(table)
    local newTable = {}
    for k, v in pairs(table)
    do
        newTable[k] = v
    end

    return newTable
end

function ArrayAdd(array, element)
    array[GetTableLength(array) + 1] = element
end

-- Project Generation =====================================================================================

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

projectToPublicIncludePaths = {}

projectToPublicDependencies = {}

projectToPublicDefines = {}

projectToLinkLibNames = {}

projectToPrecompiledLibsPath = {}

projectToAdditionalCopyCommands = {}

currentProjectsType = nil

currentProjectsSubgroup = ""

function Project:CreateProject(name, targetType, language)
    return Project:CreateProjectImpl(name, name, targetType, language)
end

function Project:CreateProjectInDirectory(directory, projectName, targetType, language)
    return Project:CreateProjectImpl(directory, projectName, targetType, language)
end

function Project:CreateProjectImpl(directory, name, targetType, language)
    local projectType = currentProjectsType
    local inProjectLocation = projectType .. "/" .. directory

    if projectType == EProjectType.ThirdParty then
        inProjectLocation = inProjectLocation .. "/" .. directory
    end
    
    newProject =
    {
        ["name"] = name,
        ["directory"] = directory,
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
                publicDefines = {},
                publicIncludePaths = {}
            },
            [EConfiguration.Development] = 
            {
                publicDependencies = {},
                privateDependencies = {},
                publicDefines = {},
                publicIncludePaths = {}
            },
            [EConfiguration.Release] = 
            {
                publicDependencies = {},
                privateDependencies = {},
                publicDefines = {},
                publicIncludePaths = {}
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

    print("Setup " .. self.name)

    if self.projectType == EProjectType.ThirdParty then
        location (self.directory)
    end

	targetdir ("../../../Binaries/" .. OutputDirectory)
	objdir ("../../../Intermediate/" .. OutputDirectory)

	libdirs ("../../../Binaries/" .. OutputDirectory)

    libdirs (self:GetAdditionalLibPaths())

    projectToPublicDependencies[self.name] = {}
    projectToPublicDefines[self.name] = {}
    projectToPublicIncludePaths[self.name] = {}
    projectToLinkLibNames[self.name] = {}
    projectToPrecompiledLibsPath[self.name] = {}
    projectToAdditionalCopyCommands[self.name] = {}

    filter "configurations:Debug"
    self:BuildConfiguration(EConfiguration.Debug, EPlatform.Windows)

    filter "configurations:Development"
    self:BuildConfiguration(EConfiguration.Development, EPlatform.Windows)

    filter "configurations:Release"
    self:BuildConfiguration(EConfiguration.Release, EPlatform.Windows)

    project(self.name).group = project(self.name).group .. currentProjectsSubgroup
end

function Project:SetupConfiguration(configuration, platform)
    
end

function Project:GetProjectFiles(configuration, platform)
    return
    {
		"**.h",
		"**.cpp",
		"**.inl",
        "**.c",
        self.name .. ".lua"
    }
end

function Project:GetAdditionalLibPaths()

end

function Project:BuildConfiguration(configuration, platform)
    self.currentConfiguration = configuration
    
    projectToLinkLibNames[self.name][configuration] = {}
    projectToAdditionalCopyCommands[self.name][configuration] = {}
    projectToPrecompiledLibsPath[self.name][configuration] = {}


    if configuration == EConfiguration.Debug then
        editAndContinue "On"
    else
        editAndContinue "Off"
    end

    -- setup function
    self:SetupConfiguration(configuration, platform)

    if #projectToLinkLibNames[self.name][configuration] == 0 then
        projectToLinkLibNames[self.name][configuration] = self.name
    end
    
    -- dependencies
    projectToPublicDependencies[self.name][configuration] = self:GetPublicDependencies(configuration)

    for dependency, _ in pairs(projectToPublicDependencies[self.name][configuration])
    do
        self:AddDependencyInternal(dependency)
    end

    for dependency, _ in pairs(self.configurations[configuration].privateDependencies)
    do
        self:AddDependencyInternal(dependency)
    end

    local privateDependenciesPublicDependencies = self:GetPrivateDependenciesPublicDependencies(configuration)
    for dependency, _ in pairs(privateDependenciesPublicDependencies)
    do
        self:AddDependencyInternal(dependency)
    end

    -- defines
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

    -- include paths
    self:AddPublicReferenceIncludePath(self.referenceProjectLocation)
    self:AddPrivateRelativeIncludePath("")

    local publicDependenciesPublicIncludePaths = self:GetPublicInlcudePathsOfPublicDependencies(configuration)
    for includePath, _ in pairs(publicDependenciesPublicIncludePaths)
    do
        self:AddIncludePathInternal(includePath)
    end

    projectToPublicIncludePaths[self.name][configuration] = self:GetThisProjectPublicIncludePaths(configuration)

    local privateDependenciesPublicIncludePath = self:GetPublicIncludePathsOfPrivateDependencies(configuration)
    for includePath, _ in pairs(privateDependenciesPublicIncludePath)
    do
        self:AddIncludePathInternal(includePath)
    end

    -- add additional includes in case if for example we want to include only headers from some project, without linking it's dll
    if self.projectType == EProjectType.Editor or self.projectType == EProjectType.Engine then
        self:AddIncludePathInternal("../../Engine")
    end

    if self.projectType == EProjectType.Editor then
        self:AddIncludePathInternal("../../Editor")
    end

    -- files
	files (self:GetProjectFiles(configuration, platform))

    -- warnings
    -- by default use normal warnings level for third party projects, and highest level for sculptor projects
    if self:AreWarningsDisabled(configuration) then
        warnings "Off"
    elseif ( self.projectType == EProjectType.ThirdParty) then
        warnings "Default"
    else
        warnings "Extra"
    end

    if self.targetType ~= ETargetType.None then
        for copyCommand, _ in pairs(projectToAdditionalCopyCommands[self.name][configuration])
        do
            prelinkcommands
            {
                copyCommand
            }
        end
        projectToAdditionalCopyCommands[self.name][self.currentConfiguration] = {}
    end
end

function Project:AddPublicDefine(define)
    -- public defines are added later in BuildConfiguration
    self.configurations[self.currentConfiguration].publicDefines[define] = true
end

function Project:AddPrivateDefine(define)
    self:AddDefineInternal(define)
end

function Project:AddPublicDependency(dependency)
    self.configurations[self.currentConfiguration].publicDependencies[dependency] = true
end

function Project:AddPrivateDependency(dependency)
    self.configurations[self.currentConfiguration].privateDependencies[dependency] = true
    self:AddDependencyInternal(dependency)
end

function Project:AddPublicRelativeIncludePath(path)
    self.configurations[self.currentConfiguration].publicIncludePaths[self.referenceProjectLocation .. path] = true
    self:AddPrivateRelativeIncludePath(path)
end

function Project:AddPublicAbsoluteIncludePath(path)
    self.configurations[self.currentConfiguration].publicIncludePaths[path] = true
    self:AddPrivateAbsoluteIncludePath(path)
end

-- adds this path only to referencing projects
function Project:AddPublicReferenceIncludePath(path)
    self.configurations[self.currentConfiguration].publicIncludePaths[path] = true
end

function Project:AddPrivateRelativeIncludePath(path)
    if self.projectType == EProjectType.ThirdParty then
        self:AddIncludePathInternal(self.name .. "/" .. path)
    else
        self:AddIncludePathInternal(path)
    end
end

function Project:AddPrivateAbsoluteIncludePath(path)
    self:AddIncludePathInternal(path)
end

function Project:GetProjectReferencePath()
    return self.referenceProjectLocation
end

function Project:DisableWarnings()
    self.configurations[self.currentConfiguration].disableWarnings = true
end

function Project:AddLinkOption(option)
    linkoptions { option }
end

function Project:AddDebugArgument(arg)
    debugargs { arg }
end

function Project:AreWarningsDisabled(configuration)
    disable = self.configurations[configuration]["disableWarnings"]
    return disable and disable == true
end

function Project:AddDefineInternal(define)
    defines { define }
end

function Project:SetLinkLibNames(names)
    projectToLinkLibNames[self.name][self.currentConfiguration] = names
end

function Project:CopyProjectLibToOutputDir(libPath)
    if self.targetType == ETargetType.None then
        localCommand = "{COPY} ".. "%{prj.location}/".. self:GetProjectReferencePath() .. libPath .. " " .. "%{cfg.buildtarget.directory}"
        projectToAdditionalCopyCommands[self.name][self.currentConfiguration][localCommand] = true
    else
        prelinkcommands
        {
            {"{COPY} %{prj.location}" .. libPath .. "%{cfg.buildtarget.directory}"}
        }
    end
end

function Project:CopyLibToOutputDir(libPath)
    if self.targetType == ETargetType.None then
        localCommand = "{COPY} " .. libPath .. " " .. "%{cfg.buildtarget.directory}"
        projectToAdditionalCopyCommands[self.name][self.currentConfiguration][localCommand] = true
    else
        prelinkcommands
        {
            {"{COPY} " .. libPath .. " %{cfg.buildtarget.directory}"}
        }
    end
end

function Project:AddDependencyInternal(dependency)
    if projectToPrecompiledLibsPath[dependencies] ~= nil then
        local precompiledLibsPath = projectToPrecompiledLibsPath[dependencies][self.currentConfiguration]
        if precompiledLibsPath ~= nil then
            libdirs { precompiledLibsPath }
        end
    end
    
    -- If dependency is a project, add it link lib names
    if projectToLinkLibNames[dependency] ~= nil then
        links { projectToLinkLibNames[dependency][self.currentConfiguration] }
        AppendTable(projectToAdditionalCopyCommands[self.name][self.currentConfiguration], projectToAdditionalCopyCommands[dependency][self.currentConfiguration])
    else
        links { dependency }
    end
end

function Project:AddIncludePathInternal(path)
    includedirs { path }
end

function Project:GetPublicDependencies(configuration)
    local allPublicDependencies = Copy(self.configurations[configuration].publicDependencies)
    for dependency, _ in pairs(self.configurations[configuration].publicDependencies)
    do
        AppendTable(allPublicDependencies, projectToPublicDependencies[dependency][configuration])
    end

    return allPublicDependencies
end

function Project:GetPrivateDependenciesPublicDependencies(configuration)
    local allPublicDependencies = {}
    for dependency, _ in pairs(self.configurations[configuration].privateDependencies)
    do
        local projectDependencies = projectToPublicDependencies[dependency]
        if projectDependencies ~= nil then
            local dependenciesForConfiguration = projectDependencies[configuration]
            if dependenciesForConfiguration ~= nil then
                AppendTable(allPublicDependencies, dependenciesForConfiguration)
            end
        end
    end

    return allPublicDependencies
end

function Project:GetThisProjectPublicDefines(configuration)
    local allPublicDefines = Copy(self.configurations[configuration].publicDefines)
    for dependency, _ in pairs(self.configurations[configuration].publicDependencies)
    do
        AppendTable(allPublicDefines, projectToPublicDefines[dependency][configuration])
    end

    return allPublicDefines
end

function Project:GetPrivateDependenciesPublicDefines(configuration)
    local allPublicDefines = {}
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

function Project:SetPrecompiledLibsPath(path)
    projectToPrecompiledLibsPath[self.name][self.currentConfiguration] = self:GetProjectReferencePath() .. path
end

function Project:AddCommonDefines(configuration, platform)
    if self.targetType == ETargetType.SharedLibrary then
        self:AddDefineInternal(string.upper(self.name) .. "_BUILD_DLL")
    end

    if configuration == EConfiguration.Debug then
        self:AddDebugDefines()
    elseif configuration == EConfiguration.Development then
        self:AddDevelopmentDefines()
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

function Project:GetThisProjectPublicIncludePaths(configuration)
    local allPublicIncludePaths = Copy(self.configurations[configuration].publicIncludePaths)
    AppendTable(allPublicIncludePaths, self:GetPublicInlcudePathsOfPublicDependencies(configuration))
    return allPublicIncludePaths
end

function Project:GetPublicInlcudePathsOfPublicDependencies(configuration)
    local allPublicIncludePaths = {}

    for dependency, _ in pairs(self.configurations[configuration].publicDependencies)
    do
        dependencyIncludePaths = projectToPublicIncludePaths[dependency]
        if dependencyIncludePaths ~= nil then
            includePathsForConfiguration = dependencyIncludePaths[configuration]
            if includePathsForConfiguration ~= nil then
                AppendTable(allPublicIncludePaths, includePathsForConfiguration)
            end
        end
    end

    return allPublicIncludePaths
end

function Project:GetPublicIncludePathsOfPrivateDependencies(configuration)
    local allIncludePaths = {}

    for dependency, _ in pairs(self.configurations[configuration].privateDependencies)
    do
        dependencyIncludePaths = projectToPublicIncludePaths[dependency]
        if dependencyIncludePaths ~= nil then
            includePathsForConfiguration = dependencyIncludePaths[configuration]
            if includePathsForConfiguration ~= nil then
                AppendTable(allIncludePaths, includePathsForConfiguration)
            end
        end
    end

    return allIncludePaths
end

-- Including projects =====================================================================================

function IncludeProject(name)
    include (currentProjectsType .. "/" .. name .. "/" .. name)
end

function IncludeProjectFromDirectory(directory, projectName)
    include (currentProjectsType .. "/" .. directory .. "/" .. projectName)
end

function SetProjectsSubgroupName(name)
    currentProjectsSubgroup = "/" .. name
end

function ResetProjectsSubgroupName(name)
    currentProjectsSubgroup = ""
end

function StartProjectsType(projectsType)
    currentProjectsType = projectsType
    ResetProjectsSubgroupName()
end

-- RHI =================================================================

ERHI=
{
    Vulkan              = "Vulkan"
}

selectedRHI = nil

function SetRHI(rhi)
    selectedRHI = rhi
end

function GetSelectedRHI()
    return selectedRHI
end

-- ShaderCompiler ======================================================

EShaderCompiler =
{
    Shaderc     = "Shaderc",
    DXC         = "DXC"
}

selectedShaderCompiler = nil

function SetShaderCompiler(shaderCompiler)
    selectedShaderCompiler = shaderCompiler
end

function GetSelectedShaderCompiler()
    return selectedShaderCompiler
end