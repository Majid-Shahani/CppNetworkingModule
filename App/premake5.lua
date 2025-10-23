project "App"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++latest"
	staticruntime "on"
	
	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")
	
	files
	{
		"src/**.hpp",
		"src/**.cpp"
	}
	
	includedirs
	{
		"%{wks.location}/Core/src",
	}
	
		links
	{
		"Core",
	}
	
	filter "system:windows"
		systemversion "latest"
		defines
		{
			"CNM_Platform_Windows"
		}

	filter "configurations:Debug"
		runtime "Debug"
		optimize "Off"
		symbols "Full"

	filter "configurations:Release"
		runtime "Release"
		optimize "On"
		symbols "Off"

	filter "configurations:Dist"
		runtime "Release"
		optimize "Full"
		symbols "Off"