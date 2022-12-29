TinyGLTF = Project:CreateProject("TinyGLTF", ETargetType.None)

function TinyGLTF:GetProjectFiles(configuration, platform)
    return
    {
        "TinyGLTF/stb_image.h",
        "TinyGLTF/tiny_gltf.h"
    }
end

TinyGLTF:SetupProject()