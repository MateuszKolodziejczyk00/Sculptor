SculptorLib = Project:CreateProject("SculptorLib", ETargetType.SharedLibrary, EProjectType.Engine)

function SculptorLib:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Math")
    self:AddPublicDependency("Spdlog")

    if platform == EPlatform.Windows then
        self:AddPrivateDependency("GLFW")

        self:AddPrivateDefine("USE_GLFW")
    end
end

SculptorLib:SetupProject()