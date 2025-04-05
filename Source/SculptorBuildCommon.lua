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

function invert_path(relative_path)
    local inverted = {}

    for _ in relative_path:gmatch("[^/]+") do
        table.insert(inverted, "..") -- Add one `..` for each directory level
    end

    return table.concat(inverted, "/") .. "/"
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
projectToPublicRelativeIncludePaths = {} -- always relative to Source folder
projectToPublicAbsoluteIncludePaths = {}

projectToPublicDependencies = {}

projectToPublicDefines = {}

projectToLinkLibNames = {}

projectToPrecompiledLibsPath = {}

projectToAdditionalAbsoluteCopyCommands = {}
projectToAdditionalRelativeCopyCommands = {}

currentProjectsType = nil

currentProjectsSubgroup = ""

internal_current_dir = nil

function Project:CreateProject(name, targetType, language)
    local dir = name
    if internal_current_dir ~= nil then
        dir = internal_current_dir
    end
    return Project:CreateProjectImpl(dir, name, targetType, language)
end

function Project:CreateProjectImpl(directory, name, targetType, language)
    local projectType = currentProjectsType
    local inProjectLocation = projectType .. "/" .. directory

    local inEngineSourceRelativePath = invert_path(inProjectLocation) -- hack: before final location beacause we need location from script, not from project

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
        engineSourceRelativePath = inEngineSourceRelativePath,
        projectLocation = inProjectLocation,
        configurations =
        {
            [EConfiguration.Debug] = {
                publicDependencies = {},
                privateDependencies = {},
                publicDefines = {},
                publicRelativeIncludePaths = {},
                publicAbsoluteIncludePaths = {}
            },
            [EConfiguration.Development] = 
            {
                publicDependencies = {},
                privateDependencies = {},
                publicDefines = {},
                publicRelativeIncludePaths = {},
                publicAbsoluteIncludePaths = {}
            },
            [EConfiguration.Release] = 
            {
                publicDependencies = {},
                privateDependencies = {},
                publicDefines = {},
                publicRelativeIncludePaths = {},
                publicAbsoluteIncludePaths = {}
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

	targetdir (self:GetEngineSourceRelativePath() .. "../Binaries/" .. OutputDirectory)
	objdir (self:GetEngineSourceRelativePath() .. "../Intermediate/" .. OutputDirectory)

	libdirs (self:GetEngineSourceRelativePath() .. "../Binaries/" .. OutputDirectory)

    libdirs (self:GetAdditionalLibPaths())

    projectToPublicDependencies[self.name] = {}
    projectToPublicDefines[self.name] = {}
    projectToPublicRelativeIncludePaths[self.name] = {}
    projectToPublicAbsoluteIncludePaths[self.name] = {}
    projectToLinkLibNames[self.name] = {}
    projectToPrecompiledLibsPath[self.name] = {}
    projectToAdditionalAbsoluteCopyCommands[self.name] = {}
    projectToAdditionalRelativeCopyCommands[self.name] = {}

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
    projectToAdditionalAbsoluteCopyCommands[self.name][configuration] = {}
    projectToAdditionalRelativeCopyCommands[self.name][configuration] = {}
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
    self:AddPublicRelativeIncludePath("")

    local publicDependenciesPublicAbsoluteIncludePaths = self:GetPublicAbsoluteInlcudePathsOfPublicDependencies(configuration)
    for includePath, _ in pairs(publicDependenciesPublicAbsoluteIncludePaths)
    do
        self:AddIncludePathInternal(includePath)
    end

    local publicDependenciesPublicRelativeIncludePaths = self:GetPublicRelativeInlcudePathsOfPublicDependencies(configuration)
    for includePath, _ in pairs(publicDependenciesPublicRelativeIncludePaths)
    do
        self:AddIncludePathInternal(self:GetEngineSourceRelativePath() .. includePath)
    end

    projectToPublicRelativeIncludePaths[self.name][configuration] = self:GetThisProjectPublicRelativeIncludePaths(configuration)
    projectToPublicAbsoluteIncludePaths[self.name][configuration] = self:GetThisProjectPublicAbsoluteIncludePaths(configuration)

    local privateDependenciesPublicAbsoluteIncludePath = self:GetPublicAbsoluteIncludePathsOfPrivateDependencies(configuration)
    for includePath, _ in pairs(privateDependenciesPublicAbsoluteIncludePath)
    do
        self:AddIncludePathInternal(includePath)
    end

    local privateDependenciesPublicRelativeIncludePath = self:GetPublicRelativeIncludePathsOfPrivateDependencies(configuration)
    for includePath, _ in pairs(privateDependenciesPublicRelativeIncludePath)
    do
        self:AddIncludePathInternal(self:GetEngineSourceRelativePath() .. includePath)
    end

    -- add additional includes in case if for example we want to include only headers from some project, without linking it's dll
    if self.projectType == EProjectType.Editor or self.projectType == EProjectType.Engine then
        self:AddIncludePathInternal(self:GetEngineSourceRelativePath() .. "Engine")
    end

    if self.projectType == EProjectType.Editor then
        self:AddIncludePathInternal(self:GetEngineSourceRelativePath() .. "Editor")
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
        for copyCommand, _ in pairs(projectToAdditionalRelativeCopyCommands[self.name][configuration])
        do
            prelinkcommands
            {
                "{COPY} " .. self:GetEngineSourceRelativePath() .. copyCommand
            }
        end
        projectToAdditionalRelativeCopyCommands[self.name][self.currentConfiguration] = {}
        for copyCommand, _ in pairs(projectToAdditionalAbsoluteCopyCommands[self.name][configuration])
        do
            prelinkcommands
            {
                copyCommand
            }
        end
        projectToAdditionalAbsoluteCopyCommands[self.name][self.currentConfiguration] = {}
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
    self.configurations[self.currentConfiguration].publicRelativeIncludePaths[self:GetProjectLocation() .. "/" .. path] = true
    self:AddPrivateRelativeIncludePath(path)
end

function Project:AddPublicAbsoluteIncludePath(path)
    self.configurations[self.currentConfiguration].publicAbsoluteIncludePaths[path] = true
    self:AddPrivateAbsoluteIncludePath(path)
end

function Project:AddPrivateRelativeIncludePath(path)
    if self.projectType == EProjectType.ThirdParty then
        self:AddIncludePathInternal(self.directory .. "/" .. path)
    else
        self:AddIncludePathInternal(path)
    end
end

function Project:AddPrivateAbsoluteIncludePath(path)
    self:AddIncludePathInternal(path)
end

function Project:GetProjectLocation()
    return self.projectLocation
end

function Project:GetEngineSourceRelativePath()
    return self.engineSourceRelativePath
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
        local localCommand = ""
        if projectType == EProjectType.ThirdParty then
            localCommand = self:GetProjectLocation() .. self.directoy .. "/" .. libPath .. " " .. "%{cfg.buildtarget.directory}"
        else
            localCommand = self:GetProjectLocation() .. libPath .. " " .. "%{cfg.buildtarget.directory}"
        end
        projectToAdditionalRelativeCopyCommands[self.name][self.currentConfiguration][localCommand] = true
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
        projectToAdditionalAbsoluteCopyCommands[self.name][self.currentConfiguration][localCommand] = true
    else
        prelinkcommands
        {
            {"{COPY} " .. libPath .. " %{cfg.buildtarget.directory}"}
        }
    end
end

function Project:AddDependencyInternal(dependency)
    if projectToPrecompiledLibsPath[dependency] ~= nil then
        local precompiledLibsPath = projectToPrecompiledLibsPath[dependency][self.currentConfiguration]
        if precompiledLibsPath ~= nil then
            for libPath, _ in pairs(precompiledLibsPath)
            do
                libdirs { self:GetEngineSourceRelativePath() .. libPath }
            end
        end
    end
    
    -- If dependency is a project, add it link lib names
    if projectToLinkLibNames[dependency] ~= nil then
        links { projectToLinkLibNames[dependency][self.currentConfiguration] }
        AppendTable(projectToAdditionalAbsoluteCopyCommands[self.name][self.currentConfiguration], projectToAdditionalAbsoluteCopyCommands[dependency][self.currentConfiguration])
        AppendTable(projectToAdditionalRelativeCopyCommands[self.name][self.currentConfiguration], projectToAdditionalRelativeCopyCommands[dependency][self.currentConfiguration])
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
        if projectToPublicDependencies[dependency] ~= nil then
            AppendTable(allPublicDependencies, projectToPublicDependencies[dependency][configuration])
        end
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
        if projectToPublicDependencies[dependency] ~= nil then
            AppendTable(allPublicDefines, projectToPublicDefines[dependency][configuration])
        end
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
    projectToPrecompiledLibsPath[self.name][self.currentConfiguration][self:GetProjectLocation() .. path] = true
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

function Project:GetThisProjectPublicAbsoluteIncludePaths(configuration)
    local allPublicAbsoluteIncludePaths = Copy(self.configurations[configuration].publicAbsoluteIncludePaths)
    AppendTable(allPublicAbsoluteIncludePaths, self:GetPublicAbsoluteInlcudePathsOfPublicDependencies(configuration))
    return allPublicAbsoluteIncludePaths
end

function Project:GetThisProjectPublicRelativeIncludePaths(configuration)
    local allPublicRelativeIncludePaths = Copy(self.configurations[configuration].publicRelativeIncludePaths)
    AppendTable(allPublicRelativeIncludePaths, self:GetPublicRelativeInlcudePathsOfPublicDependencies(configuration))
    return allPublicRelativeIncludePaths
end

function Project:GetPublicAbsoluteInlcudePathsOfPublicDependencies(configuration)
    local allPublicIncludePaths = {}

    for dependency, _ in pairs(self.configurations[configuration].publicDependencies)
    do
        dependencyIncludePaths = projectToPublicAbsoluteIncludePaths[dependency]
        if dependencyIncludePaths ~= nil then
            includePathsForConfiguration = dependencyIncludePaths[configuration]
            if includePathsForConfiguration ~= nil then
                AppendTable(allPublicIncludePaths, includePathsForConfiguration)
            end
        end
    end

    return allPublicIncludePaths
end

function Project:GetPublicRelativeInlcudePathsOfPublicDependencies(configuration)
    local allPublicIncludePaths = {}

    for dependency, _ in pairs(self.configurations[configuration].publicDependencies)
    do
        dependencyIncludePaths = projectToPublicRelativeIncludePaths[dependency]
        if dependencyIncludePaths ~= nil then
            includePathsForConfiguration = dependencyIncludePaths[configuration]
            if includePathsForConfiguration ~= nil then
                AppendTable(allPublicIncludePaths, includePathsForConfiguration)
            end
        end
    end

    return allPublicIncludePaths
end

function Project:GetPublicAbsoluteIncludePathsOfPrivateDependencies(configuration)
    local allIncludePaths = {}

    for dependency, _ in pairs(self.configurations[configuration].privateDependencies)
    do
        dependencyIncludePaths = projectToPublicAbsoluteIncludePaths[dependency]
        if dependencyIncludePaths ~= nil then
            includePathsForConfiguration = dependencyIncludePaths[configuration]
            if includePathsForConfiguration ~= nil then
                AppendTable(allIncludePaths, includePathsForConfiguration)
            end
        end
    end

    return allIncludePaths
end

function Project:GetPublicRelativeIncludePathsOfPrivateDependencies(configuration)
    local allIncludePaths = {}

    for dependency, _ in pairs(self.configurations[configuration].privateDependencies)
    do
        dependencyIncludePaths = projectToPublicRelativeIncludePaths[dependency]
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
    internal_current_dir = directory
    include (currentProjectsType .. "/" .. directory .. "/" .. projectName)
    internal_current_dir = nil
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