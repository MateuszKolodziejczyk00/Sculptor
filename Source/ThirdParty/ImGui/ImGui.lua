ImGui = Project:CreateProject("ImGui", ETargetType.SharedLibrary)

function ImGui:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Eigen")

    self:AddPublicRelativeIncludePath("/../Sculptor")

    self:AddPublicDefine("IMGUI_USER_CONFIG=\"SculptorImGuiConfig.h\"")

    self:AddPrivateDefine("IMGUI_BUILD_DLL=1");

    if platform == EPlatform.Windows then
        self:AddPublicDefine("IMGUI_PLATFORM_WINDOWS")
    end
end

function ImGui:GetProjectFiles(configuration, platform)
    return
    {
        "ImGui/*.h",
        "ImGui/*.cpp",
        "Sculptor/SculptorImGuiConfig.h"
    }
end

ImGui:SetupProject()