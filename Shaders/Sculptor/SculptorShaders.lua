SculptorShaders = Project:CreateProject("SculptorShaders", ETargetType.None)

function SculptorShaders:GetProjectFiles(configuration, platform)
    return
    {
        "**.hlsl",
        "**.hlsli",
    }
end

SculptorShaders:SetupProject()