SculptorLib = Project:CreateProject("SculptorLib", ETargetType.SharedLibrary, EProjectType.Engine)

function SculptorLib:GetPrivateIncludePaths()
    return
    {
        "Source/ThirdParty/Eigen"
    }
end

SculptorLib:SetupProject()