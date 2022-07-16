Volk = Project:CreateProject("Volk", ETargetType.None, EProjectType.ThirdParty, ELanguage.C)

function Volk:GetProjectFiles(configuration, platform)
    return
    {
        "Volk/volk.h"
    }
end

Volk:SetupProject()