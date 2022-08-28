SculptorShaders = Project:CreateProject("SculptorShaders", ETargetType.None)

function SculptorShaders:GetProjectFiles(configuration, platform)
    return
    {
        "../Shaders/**.glsl",
        "../Shaders/**.glslh",
    }
end

SculptorShaders:SetupProject()