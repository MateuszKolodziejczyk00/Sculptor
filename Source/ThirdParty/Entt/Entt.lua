Entt = Project:CreateProject("Entt", ETargetType.None)

function Entt:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("/src")
end

function Entt:GetProjectFiles(configuration, platform)
    return
    {
        "Entt/src/**.h",
        "Entt/natvis/**.natvis"
    }
end

Entt:SetupProject()