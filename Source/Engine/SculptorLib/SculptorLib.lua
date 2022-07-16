SculptorLib = Project:CreateProject("SculptorLib", ETargetType.SharedLibrary, EProjectType.Engine)

function SculptorLib:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Math")

    if platform == EPlatform.Windows then
        self:AddPrivateDependency("GLFW")
    end
end

SculptorLib:SetupProject()