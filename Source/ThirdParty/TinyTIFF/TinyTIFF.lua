local scriptDirectory = path.getdirectory(_SCRIPT)

local function ReadFile(path)
    local file = io.open(path, "r")
    if file == nil then
        return nil
    end

    local content = file:read("*a")
    file:close()

    return content
end

local function WriteFile(path, content)
    local existingContent = ReadFile(path)
    if existingContent == content then
        return
    end

    local file = assert(io.open(path, "w"))
    file:write(content)
    file:close()
end

local function GenerateTinyTIFFHeaders()
    local generatedDirectory = scriptDirectory .. "/Generated"
    local compileTime = os.date("%Y-%m-%d %H:%M:%S")
    local version = "unknown"
    local cmakeLists = ReadFile(scriptDirectory .. "/TinyTIFF/CMakeLists.txt")
    if cmakeLists ~= nil then
        version = cmakeLists:match("project%s*%(%s*libTinyTIFF%s+VERSION%s+([%d%.]+)") or version
    end

    os.mkdir(generatedDirectory)

    WriteFile(generatedDirectory .. "/tinytiff_export.h", [[#ifndef TINYTIFF_EXPORT_H
#define TINYTIFF_EXPORT_H

#define TINYTIFF_EXPORT

#endif // TINYTIFF_EXPORT_H
]])

    WriteFile(generatedDirectory .. "/tinytiff_version.h", string.format([[#ifndef TINYTIFF_VERSION_DEFINES_H
#define TINYTIFF_VERSION_DEFINES_H

#define TINYTIFF_VERSION "%s"
#define TINYTIFF_COMPILETIME "%s"
#define TINYTIFF_GITVERSION "unknown"

#define TINYTIFF_FULLVERSION "%s (%s, Git: unknown)"

#endif // TINYTIFF_VERSION_DEFINES_H
]], version, compileTime, version, compileTime))
end

GenerateTinyTIFFHeaders()

TinyTIFF = Project:CreateProject("TinyTIFF", ETargetType.StaticLibrary)


function TinyTIFF:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("/src")
    self:AddPublicRelativeIncludePath("../Generated")

    if platform == EPlatform.Windows then
        self:AddPrivateDefine("HAVE_FTELLI64")
        self:AddPrivateDefine("HAVE_FSEEKI64")
        self:AddPrivateDefine("HAVE_STRCPY_S")
        self:AddPrivateDefine("HAVE_STRCAT_S")
        self:AddPrivateDefine("HAVE_SPRINTF_S")
    end

	self:DisableWarnings()
end


function TinyTIFF:GetProjectFiles(configuration, platform)
    return
    {
        "Generated/**.h",
        "TinyTIFF/src/**.h",
        "TinyTIFF/src/**.c",
    }
end


TinyTIFF:SetupProject()
