DDS = Project:CreateProject("DDS", ETargetType.None, ELanguage.C)

function DDS:GetProjectFiles(configuration, platform)
    return
    {
        "DDS/dds.h"
    }
end

DDS:SetupProject()