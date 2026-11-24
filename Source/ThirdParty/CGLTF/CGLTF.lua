CGLTF = Project:CreateProject("CGLTF", ETargetType.None)

function CGLTF:GetProjectFiles(configuration, platform)
    return
    {
        "CGLTF/cgltf.h"
    }
end

CGLTF:SetupProject()
