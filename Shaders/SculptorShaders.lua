SculptorShaders = Project:CreateProject("SculptorShaders", ETargetType.None)

function SculptorShaders:GetProjectFiles(configuration, platform)
    return
    {
        "../Shaders/**.hlsl",
        "../Shaders/**.hlsli",
    }
end

SculptorShaders:SetupProject()