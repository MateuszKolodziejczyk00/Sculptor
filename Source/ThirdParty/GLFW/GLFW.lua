GLFW = Project:CreateProject("GLFW", ETargetType.StaticLibrary, EProjectType.ThirdParty, ELanguage.C)

function GLFW:GetSourceFilesRelativeLocation()
    return "/include"
end

GLFW:SetupProject()