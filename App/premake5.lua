project "App"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++latest"
	staticruntime "on"
	
	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")
	
	files
	{
		"src/**.h",
		"src/**.c",
		"src/**.hpp",
		"src/**.cpp"
	}
	
	includedirs
	{
		"%{wks.location}/Core/include",
		"%{wks.location}/%{prj.name}/src",
	}
	
		links
	{
		"Core",
	}
	
	filter "system:windows"
		systemversion "latest"
		defines
		{
			"CL_Platform_Windows"
		}
		links
		{
			"Ws2_32.lib"
		}
	filter "system:linux"
		defines
		{
			"CL_Platform_Linux"
		}
	filter "system:macosx"
		defines
		{
			"CL_Platform_Mac"
		}

	filter "configurations:Debug"
		defines { "CL_DEBUG", "CL_ENABLE_ASSERTS" }
		runtime "Debug"
		optimize "Off"
		symbols "Full"

	filter "configurations:Release"
		defines "CL_RELEASE"
		runtime "Release"
		optimize "On"
		symbols "Off"

	filter "configurations:Dist"
		defines "CL_DIST"
		runtime "Release"
		optimize "Full"
		symbols "Off"