GoogleTest = Project:CreateProject("GoogleTest", ETargetType.StaticLibrary)

function GoogleTest:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("/googletest/include")

    self:AddPrivateRelativeIncludePath("/googletest")
end

function GoogleTest:GetProjectFiles(configuration, platform)
    return 
    {
        "GoogleTest/googletest/include/**.h",

        "GoogleTest/googletest/src/gtest-assertion-result.cc",
        "GoogleTest/googletest/src/gtest-death-test.cc",
        "GoogleTest/googletest/src/gtest-filepath.cc",
        "GoogleTest/googletest/src/gtest-matchers.cc",
        "GoogleTest/googletest/src/gtest-port.cc",
        "GoogleTest/googletest/src/gtest-printers.cc",
        "GoogleTest/googletest/src/gtest-test-part.cc",
        "GoogleTest/googletest/src/gtest-typed-test.cc",
        "GoogleTest/googletest/src/gtest.cc"
    }
end

GoogleTest:SetupProject()