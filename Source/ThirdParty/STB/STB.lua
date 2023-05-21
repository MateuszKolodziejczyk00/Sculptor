STB = Project:CreateProject("STB", ETargetType.None)

function STB:GetProjectFiles(configuration, platform)
    return
    {
        "STB/*.h"
    }
end

STB:SetupProject()