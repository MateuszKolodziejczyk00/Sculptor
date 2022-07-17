Volk = Project:CreateProject("Volk", ETargetType.None, ELanguage.C)

function Volk:GetProjectFiles(configuration, platform)
    return
    {
        "Volk/volk.h"
    }
end

Volk:SetupProject()