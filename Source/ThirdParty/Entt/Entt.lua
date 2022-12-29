Entt = Project:CreateProject("Entt", ETargetType.None)

function Entt:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("/single_include")
end

function Entt:GetProjectFiles(configuration, platform)
    return
    {
        "Entt/single_include/**.h"
    }
end

Entt:SetupProject()